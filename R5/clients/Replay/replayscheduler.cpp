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
{
    std::ifstream fp;
    fp.open(schedulePath.c_str());
    m_schedule = WebCore::EventActionSchedule::deserialize(fp);
    fp.close();

    ActionLogStrictMode(false); // replaying does not play well with the action log

    m_eventActionTimeoutTimer.setInterval(10 * 1000); // an event action must be executed within 10 seconds
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

    // Detect relax token

    if (nextToSchedule.isNull()) {
        std::cout << "Entered relaxed replay mode" << std::endl;

        m_timeProvider->setMode(BEST_EFFORT);
        m_randomProvider->setMode(BEST_EFFORT);
        m_networkProvider->setMode(BEST_EFFORT);
        m_mode = BEST_EFFORT;

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

    // Relaxed execution

    if (!found && m_mode == BEST_EFFORT)


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

    if (found) {
        m_schedule->remove(0);

        m_eventActionTimeoutTimer.stop();

        if (m_schedule->isEmpty()) {
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

    if (m_mode == STRICT) {
        // FATAL, this must not happen

        std::cerr << std::endl << "Error: Failed execution schedule after waiting for 10 seconds..." << std::endl;
        std::cerr << "This is the current queue of events" << std::endl;
        debugPrintTimers(WebCore::threadGlobalData().threadTimers().eventActionRegister());

        std::exit(1);
    }

    WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Could not replay schedule", "");

    m_replaySuccessful = false;
    stop();
}

bool ReplayScheduler::isFinished()
{
    return m_schedule->isEmpty();
}

void ReplayScheduler::debugPrintTimers(WebCore::EventActionRegister* eventActionRegister)
{
    std::cout << "=========== TIMERS ===========" << std::endl;
    std::cout << "RELAXED MODE: " << (m_mode == BEST_EFFORT ? "Yes" : "No") << std::endl;
    std::cout << "NEXT -> " << m_schedule->first().second.toString() << std::endl;
    std::cout << "QUEUE -> " << std::endl;

    eventActionRegister->debugPrintNames();

}
