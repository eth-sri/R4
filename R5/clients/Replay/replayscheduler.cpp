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
#include "platform/schedule/EventActionRegister.h"
#include "wtf/ActionLogReport.h"
#include "wtf/warningcollectorreport.h"

#include "fuzzyurl.h"

#include "replayscheduler.h"

ReplayScheduler::ReplayScheduler(const std::string& schedulePath, QNetworkReplyControllableFactoryReplay* networkProvider, TimeProviderReplay* timeProvider, RandomProviderReplay* randomProvider)
    : QObject(NULL)
    , Scheduler()
    , m_networkProvider(networkProvider)
    , m_timeProvider(timeProvider)
    , m_randomProvider(randomProvider)
    , m_mode(STRICT)
    , m_replaySuccessful(true)
    , m_skipEventActionsUntilHit(false)
    , m_timeout_miliseconds(4000)
{
    std::ifstream fp;
    fp.open(schedulePath.c_str());
    m_schedule = WebCore::EventActionSchedule::deserialize(fp);
    fp.close();

    ActionLogStrictMode(false); // replaying does not play well with the action log

    m_eventActionTimeoutTimer.setInterval(m_timeout_miliseconds); // an event action must be executed within x miliseconds
    m_eventActionTimeoutTimer.setSingleShot(true);
    connect(&m_eventActionTimeoutTimer, SIGNAL(timeout()), this, SLOT(slEventActionTimeout()));
}

ReplayScheduler::~ReplayScheduler()
{
    delete m_schedule;
}

void ReplayScheduler::eventActionScheduled(const WTF::EventActionDescriptor&,
                                           WebCore::EventActionRegister*)
{
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
        emit sigDone();
        return false;
    }

    WTF::EventActionDescriptor nextToSchedule = m_schedule->first().second;

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
        }

        m_schedule->remove(0);

        if (m_schedule->isEmpty()) {
            emit sigDone();
            return false;
        }

        nextToSchedule = m_schedule->first().second;
    }

    // Exact execution

    std::string eventActionType = nextToSchedule.getType();

    m_timeProvider->setCurrentDescriptorString(QString::fromStdString(nextToSchedule.toString()));
    m_randomProvider->setCurrentDescriptorString(QString::fromStdString(nextToSchedule.toString()));

    bool found = eventActionRegister->runEventAction(nextToSchedule);

    m_timeProvider->unsetCurrentDescriptorString();
    m_randomProvider->unsetCurrentDescriptorString();

    // Relaxed non-deterministic execution

    if (!found && (m_mode == BEST_EFFORT || m_mode == BEST_EFFORT_NOND)) {


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
          * If we have multiple matches then we use the match score by fuzzyurl.
          *
          * This solves the problem of URLs with timestamps.
          *
          */

        if (eventActionType == "Network" || eventActionType == "HTMLDocumentParser" || eventActionType == "DOMTimer" || eventActionType == "BrowserLoadUrl") {

            QString url = QString::fromStdString(nextToSchedule.getParameter(0));

            unsigned long sequenceNumber1 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "HTMLDocumentParser") ? QString::fromStdString(nextToSchedule.getParameter(1)).toULong() : 0;
            unsigned long sequenceNumber2 = (eventActionType == "Network" || eventActionType == "DOMTimer") ? QString::fromStdString(nextToSchedule.getParameter(2)).toULong() : 0;
            unsigned long sequenceNumber3 = (eventActionType == "DOMTimer") ? QString::fromStdString(nextToSchedule.getParameter(3)).toULong() : 0;

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

                unsigned long candidateSequenceNumber1 = (eventActionType == "Network" || eventActionType == "DOMTimer" || eventActionType == "HTMLDocumentParser") ? QString::fromStdString(candidate.getParameter(1)).toULong() : 0;
                unsigned long candidateSequenceNumber2 = (eventActionType == "Network" || eventActionType == "DOMTimer") ? QString::fromStdString(candidate.getParameter(2)).toULong() : 0;
                unsigned long candidateSequenceNumber3 = (eventActionType == "DOMTimer") ? QString::fromStdString(candidate.getParameter(3)).toULong() : 0;

                if (candidateSequenceNumber1 != sequenceNumber1 || candidateSequenceNumber2 != sequenceNumber2 || candidateSequenceNumber3 != sequenceNumber3) {
                    continue;
                }

                unsigned int score = matcher->score(QUrl(QString::fromStdString(candidate.getParameter(0))));

                if (score > bestScore) {
                    bestScore = score;
                    bestDescriptor = candidate;
                }
            }

            if (bestScore > 0) {

                std::stringstream details;
                details << nextToSchedule.toString() << " fuzzy matched with " << bestDescriptor.toString() << " (score " << bestScore << ")";

                std::cout << "Fuzzy match: " << details.str() << std::endl;
                WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Event action fuzzy matched in best effort mode.", details.str());

                m_timeProvider->setCurrentDescriptorString(QString::fromStdString(bestDescriptor.toString()));
                m_randomProvider->setCurrentDescriptorString(QString::fromStdString(bestDescriptor.toString()));

                found = eventActionRegister->runEventAction(bestDescriptor);

                m_timeProvider->unsetCurrentDescriptorString();
                m_randomProvider->unsetCurrentDescriptorString();
            }
        }

    } // End fuzzy non-deterministic match

    // Relaxed execution

    if (!found && m_mode == BEST_EFFORT) {

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

        // This step is implemented partially in the shutdown logic (see the next
        // code block for detection of new events) and in the timeout handler
        // (see slEventActionTimeout for deletion of missing event actions).

    }

    if (!found && m_skipEventActionsUntilHit) {
        // We could not find the current event action in the replay schedule. Skip
        // it and emit a warning.

        // Notice that we will continue skipping event actions until we get a hit.

        std::stringstream detail;
        detail << "This is the current queue of events" << std::endl;
        debugPrintTimers(detail, WebCore::threadGlobalData().threadTimers().eventActionRegister());

        WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Event action skipped after timeout.", detail.str());

        m_schedule->remove(0);

        return true; // Go to the next event action now
    }

    if (found) {

        m_schedule->remove(0);
        m_skipEventActionsUntilHit = false;
        m_eventActionTimeoutTimer.stop();

        if (m_schedule->isEmpty()) {

            // Emit all remaining event actions as potential errors
            // Notice, that we do not record the remaining event actions in the original
            // exection. Thus, this will be an over approximation of the actual list of
            // new event actions.

            std::set<std::string> names = eventActionRegister->getWaitingNames();
            std::set<std::string>::const_iterator iter;

            for (iter = names.begin(); iter != names.end(); iter++) {
                WTF::WarningCollectorReport("WEBERA_SCHEDULER", "New event action observed.", (*iter));
            }

            // Stop scheduler and sub systems

            stop();
        }

        return true;
    }

    if (!m_eventActionTimeoutTimer.isActive()) {
        m_eventActionTimeoutTimer.start(); // start timeout for this event action
    }

    return false;
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
        m_replaySuccessful = false;
        stop();

        break;
    }


    case BEST_EFFORT:
        // Skip this event action, and enter fast forward mode (skipping any subsequent event actions
        // in the replay schedule that are not enabled.

        m_skipEventActionsUntilHit = true;

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

void ReplayScheduler::debugPrintTimers(std::ostream& out, WebCore::EventActionRegister* eventActionRegister)
{
    out << "=========== TIMERS ===========" << std::endl;
    out << "RELAXED MODE: " << (m_mode == BEST_EFFORT ? "Yes" : "No") << std::endl;
    out << "NEXT -> " << m_schedule->first().second.toString() << std::endl;
    out << "QUEUE -> " << std::endl;

    eventActionRegister->debugPrintNames(out);

}
