/*
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "wtf/ExportMacros.h"
#include "platform/ThreadGlobalData.h"
#include "platform/ThreadTimers.h"
#include "wtf/ActionLogReport.h"
#include "WebCore/platform/EventActionHappensBeforeReport.h"

#include "fuzzyurl.h"

#include "replayscheduler.h"

ReplayScheduler::ReplayScheduler(const std::string& schedulePath, QNetworkReplyControllableFactoryReplay* networkProvider, TimeProviderReplay* timeProvider, RandomProviderReplay* randomProvider)
    : QObject(NULL)
    , Scheduler()
    , m_networkProvider(networkProvider)
    , m_timeProvider(timeProvider)
    , m_randomProvider(randomProvider)
    , m_mode(STRICT)
    , m_state(RUNNING)
    , m_doneEmitted(false)
    , m_skipAfterNextTry(false)
    , m_timeout_use_aggressive(false)
    , m_timeout_miliseconds(20000)
    , m_timeout_aggressive_miliseconds(500)
    , m_nextEventActionId(WebCore::HBAllocateEventActionId())
{
    std::ifstream fp;
    fp.open(schedulePath.c_str());
    m_schedule = WebCore::EventActionSchedule::deserialize(fp);
    m_schedule->reverse();
    fp.close();

    m_eventActionTimeoutTimer.setInterval(m_timeout_miliseconds); // an event action must be executed within x miliseconds
    m_eventActionTimeoutTimer.setSingleShot(true);
    connect(&m_eventActionTimeoutTimer, SIGNAL(timeout()), this, SLOT(slEventActionTimeout()));
}

ReplayScheduler::~ReplayScheduler()
{
    delete m_schedule;
}

void ReplayScheduler::eventActionScheduled(const WTF::EventActionDescriptor&,
                                           WebCore::EventActionRegister* eventActionRegister)
{
    while (executeDelayedEventAction(eventActionRegister)) {
        continue;
    }
}

void ReplayScheduler::executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister)
{
    while (executeDelayedEventAction(eventActionRegister)) {
        continue;
    }
}

bool ReplayScheduler::executeDelayedEventAction(WebCore::EventActionRegister* eventActionRegister)
{
    if (m_schedule->isEmpty() || m_mode == STOP) {
        stop(FINISHED, eventActionRegister);
        return false;
    }

    for (size_t i = 0; i < m_schedule_backlog.size(); ++i) {
        ActionLogStrictMode(false);
        bool success = tryExecuteEventActionDescriptor(eventActionRegister, m_schedule_backlog[i]);
        ActionLogStrictMode(true);
        if (success) {
            m_schedule_backlog.remove(i);
            WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Event action executed from pending schedule.", "");
            return true;
        }
    }

    bool success = tryExecuteEventActionDescriptor(eventActionRegister, m_schedule->last());

    if (success) {
        m_schedule->removeLast();

        m_skipAfterNextTry = false;
        m_eventActionTimeoutTimer.stop();

        return true;

    } else if (m_skipAfterNextTry) {

        /**
          * Relaxed execution, when we did not find anything using a fuzzy match.
          *
          * When a replay deviates, compared to the original execution, it is
          * possible that previously unknown event actions are scheduled, and
          * previously known event actions are never scheduled or delayed.
          *
          * Missing event actions we can skip (and monitor if they appear later).
          *
          * New event actions are ignored (with the hope that they are not needed
          * to enable any of the old event actions).
          * This can be the case if these new event actions e.g. block load events.
          *
          */

        // We could not find the current event action in the replay schedule. Skip
        // it and emit a warning.

        // Notice that we will continue skipping event actions until we get a hit.

        std::stringstream detail;
        detail << "This is the current queue of events" << std::endl;
        debugPrintTimers(detail, WebCore::threadGlobalData().threadTimers().eventActionRegister());

        WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Event action skipped after timeout.", detail.str());

        m_schedule_backlog.append(m_schedule->last());
        m_schedule->removeLast();

        return true; // Go to the next event action now

    }

    if (!m_eventActionTimeoutTimer.isActive()) {

        const WebCore::EventActionScheduleItem& item = m_schedule->last();
        const WTF::EventActionDescriptor& nextToSchedule = item.second;
        const std::string& eventActionType = nextToSchedule.getType();

        if (eventActionType == "DOMTimer") {
            // set timeout to match expected time to trigger the next DOMTimer
            m_eventActionTimeoutTimer.setInterval(QString::fromStdString(nextToSchedule.getParameter(2)).toULong() + m_timeout_aggressive_miliseconds);
        } else {
            m_eventActionTimeoutTimer.setInterval(m_mode == BEST_EFFORT ? m_timeout_aggressive_miliseconds : m_timeout_miliseconds);
        }

        m_eventActionTimeoutTimer.start(); // start timeout for this event action

    }

    return false;

}

bool ReplayScheduler::tryExecuteEventActionDescriptor(
        WebCore::EventActionRegister* eventActionRegister,
        const WebCore::EventActionScheduleItem& next)
{

    const WTF::EventActionDescriptor& nextToSchedule = next.second;
    WTF::EventActionId nextToScheduleId = next.first;

    // Detect relax non-determinism token and relax token. We assume that the first occurrence of a token
    // is always the non-determinism one, and the second (and thrid and ...) are relax tokens.

    if (nextToSchedule.isNull()) {

        if (m_mode == STRICT) {
            std::cout << "Entered relaxed non-deterministic replay mode" << std::endl;
            m_timeProvider->setMode(BEST_EFFORT_NOND);
            m_randomProvider->setMode(BEST_EFFORT_NOND);
            m_networkProvider->setMode(BEST_EFFORT_NOND);
            m_mode = BEST_EFFORT_NOND;
        } else {
            std::cout << "Entered relaxed replay mode" << std::endl;
            m_timeProvider->setMode(BEST_EFFORT);
            m_randomProvider->setMode(BEST_EFFORT);
            m_networkProvider->setMode(BEST_EFFORT);
            m_mode = BEST_EFFORT;
            m_eventActionTimeoutTimer.setInterval(m_timeout_aggressive_miliseconds);
        }

        return true;
    }

    std::string eventActionType = nextToSchedule.getType();

    WTF::WarningCollectorSetCurrentEventAction(m_nextEventActionId);

    // Patch event action descriptor if it references old event action IDs
    // For now, only DOM timer (index 4) use this feature

    if (eventActionType == "DOMTimer" && !nextToSchedule.isPatched()) {
        int oldId = atoi(nextToSchedule.getParameter(4).c_str());

        std::stringstream param;
        param << eventActionRegister->translateOldIdToNew(oldId);

        std::cout << "Translating " << nextToSchedule.toString() << " from " << oldId << " to " << param.str() << std::endl;

        nextToSchedule.patchParameter(4, param.str());
    }

    // Exact execution

    m_timeProvider->setCurrentDescriptorString(QString::fromStdString(nextToSchedule.toUnpatchedString()));
    m_randomProvider->setCurrentDescriptorString(QString::fromStdString(nextToSchedule.toUnpatchedString()));

    bool found = eventActionRegister->runEventAction(m_nextEventActionId, nextToScheduleId, nextToSchedule);

    m_timeProvider->unsetCurrentDescriptorString();
    m_randomProvider->unsetCurrentDescriptorString();

    // Relaxed non-deterministic execution

    if (!found && (m_skipAfterNextTry && m_mode == BEST_EFFORT_NOND)) {
        // Important that we only do fuzzy matching just before giving up...
        // If we always fuzzy match, then we could decide to fuzzy match with a bad candidate, while a better candidate would
        // appear if we waited for skipAfterNextTry

        // Try to do a fuzzy match

        /**
          * Fuzzy match the URL part of various event actions
          * (Currently Network, HTMLDocumentParser, DOMTimer and BrowserLoadUrl)
          *
          * With event action A, we will match an event action B iff
          *
          * Handled by fuzzyurl:
          *     A's domain matches B's domain
          *     A's fragments (before ?) matches B's fragments
          *     A's keywords in query (?KEYWORD=VALUE) matches B's keywords
          *
          * Network event actions:
          *     A's url sequence number and segment number matches B's ...
          *
          * HTMLDocumentParser event actions:
          *     A's segment number matches B's segment number
          *
          * DOMTimer event actions:
          *     A's line number, timeout, isSingleshot, [skip], and same url sequence matches B's ...
          *
          * ScriptRunner event actions:
          *     A's async mode, and type matches B's ...
          *
          * If we have multiple matches then we use the match score by fuzzyurl.
          *
          * This solves the problem of URLs with timestamps.
          *
          */

        if (eventActionType == "Network" || eventActionType == "HTMLDocumentParser" || eventActionType == "DOMTimer" ||
                eventActionType == "BrowserLoadUrl" || eventActionType == "ScriptRunner") {

            QString url = QString::fromStdString(nextToSchedule.getParameter(0));

            unsigned long sequenceNumber1 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "HTMLDocumentParser" || eventActionType == "ScriptRunner") ? QString::fromStdString(nextToSchedule.getParameter(1)).toULong() : 0;
            unsigned long sequenceNumber2 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "ScriptRunner") ? QString::fromStdString(nextToSchedule.getParameter(2)).toULong() : 0;
            unsigned long sequenceNumber3 = (eventActionType == "DOMTimer") ? QString::fromStdString(nextToSchedule.getParameter(3)).toULong() : 0;
            unsigned long sequenceNumber4 = (eventActionType == "DOMTimer") ? QString::fromStdString(nextToSchedule.getParameter(4)).toULong() : 0; // DOMTimer's parent ID, fuzzy match this one
            unsigned long sequenceNumber5 = (eventActionType == "DOMTimer") ? QString::fromStdString(nextToSchedule.getParameter(5)).toULong() : 0;

            FuzzyUrlMatcher* matcher = new FuzzyUrlMatcher(QUrl(url));

            std::set<std::string> names = eventActionRegister->getWaitingNames();
            std::set<std::string>::const_iterator iter;

            unsigned int bestScore = 0;
            WTF::EventActionDescriptor bestDescriptor;

            for (iter = names.begin(); iter != names.end(); iter++) {

                WTF::EventActionDescriptor candidate = WTF::EventActionDescriptor::deserialize((*iter));

                if (candidate.getType() != eventActionType) {
                    continue;
                }

                unsigned long candidateSequenceNumber1 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "HTMLDocumentParser" || eventActionType == "ScriptRunner") ? QString::fromStdString(candidate.getParameter(1)).toULong() : 0;
                unsigned long candidateSequenceNumber2 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "ScriptRunner") ? QString::fromStdString(candidate.getParameter(2)).toULong() : 0;
                unsigned long candidateSequenceNumber3 = (eventActionType == "DOMTimer") ? QString::fromStdString(candidate.getParameter(3)).toULong() : 0;
                unsigned long candidateSequenceNumber4 = (eventActionType == "DOMTimer") ? QString::fromStdString(candidate.getParameter(4)).toULong() : 0;
                unsigned long candidateSequenceNumber5 = (eventActionType == "DOMTimer") ? QString::fromStdString(candidate.getParameter(5)).toULong() : 0;

                if (candidateSequenceNumber1 != sequenceNumber1 || candidateSequenceNumber2 != sequenceNumber2 || candidateSequenceNumber3 != sequenceNumber3 ||
                        candidateSequenceNumber5 != sequenceNumber5) {
                    continue;
                }

                unsigned int score = matcher->score(QUrl(QString::fromStdString(candidate.getParameter(0))));

                if (sequenceNumber4 != 0) {
                    score = score / 2;

                    if (sequenceNumber4 == candidateSequenceNumber4) {
                        score += UINT_MAX / 2;
                    }
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestDescriptor = candidate;
                }
            }

            if (bestScore > 0) {

                std::stringstream details;
                details << nextToSchedule.toString() << " (expected) fuzzy matched with " << bestDescriptor.toString() << " (score " << bestScore << ")";
                details << "This is the current queue of events" << std::endl;
                debugPrintTimers(details, WebCore::threadGlobalData().threadTimers().eventActionRegister());

                std::cout << "Fuzzy match: " << details.str() << std::endl;
                WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Event action fuzzy matched in best effort mode.", details.str());

                m_timeProvider->setCurrentDescriptorString(QString::fromStdString(bestDescriptor.toUnpatchedString()));
                m_randomProvider->setCurrentDescriptorString(QString::fromStdString(bestDescriptor.toUnpatchedString()));

                found = eventActionRegister->runEventAction(m_nextEventActionId, nextToScheduleId, bestDescriptor);

                m_timeProvider->unsetCurrentDescriptorString();
                m_randomProvider->unsetCurrentDescriptorString();
            }
        }

    } // End fuzzy non-deterministic match

    if (found) {
        m_nextEventActionId = WebCore::HBAllocateEventActionId();
    }

    return found;

}

void ReplayScheduler::slEventActionTimeout()
{
    if (m_schedule->isEmpty()) {
        return;
    }

    switch (m_mode) {

    case STRICT: {

        std::cerr << std::endl << "Error: Failed execution schedule after waiting for " << m_timeout_miliseconds << " miliseconds." << std::endl;
        std::cerr << "This is the current queue of events" << std::endl;
        debugPrintTimers(std::cerr, WebCore::threadGlobalData().threadTimers().eventActionRegister());

        std::exit(1);

        break;
    }

    case BEST_EFFORT_NOND: {
        // This should not happen

        std::stringstream detail;
        detail << "Error: Failed execution schedule after waiting for " << m_timeout_miliseconds << " miliseconds..." << std::endl;
        detail << "This is the current queue of events" << std::endl;
        debugPrintTimers(detail, WebCore::threadGlobalData().threadTimers().eventActionRegister());

        WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Could not replay schedule while in non-deterministic relax mode", detail.str());
        stop(ERROR);

        break;
    }


    case BEST_EFFORT:
        // Skip this event action, and enter fast forward mode (skipping any subsequent event actions
        // in the replay schedule that are not enabled.

        m_skipAfterNextTry = true;

        break;

    case STOP:
        break;

    default:
        ASSERT_NOT_REACHED();

    }
}

bool ReplayScheduler::isFinished()
{
    return m_schedule->isEmpty();
}

void ReplayScheduler::timeout()
{
    WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Scheduler timed out.", "");
    stop(TIMEOUT);
}

void ReplayScheduler::debugPrintTimers(std::ostream& out, WebCore::EventActionRegister* eventActionRegister)
{
    out << "=========== TIMERS ===========" << std::endl;
    out << "RELAXED MODE: " << (m_mode == BEST_EFFORT ? "Yes" : "No") << std::endl;
    out << "NEXT -> " << m_schedule->last().second.toString() << std::endl;
    out << "QUEUE -> " << std::endl;

    eventActionRegister->debugPrintNames(out);

}
