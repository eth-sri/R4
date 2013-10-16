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

#include "platform/schedule/EventActionRegister.h"

#include "fuzzyurl.h"

#include "replayscheduler.h"

ReplayScheduler::ReplayScheduler(const std::string& schedulePath)
    : QObject(NULL)
    , Scheduler()
    , m_scheduleWaits(0)
{
    std::ifstream fp;
    fp.open(schedulePath);
    m_schedule = WebCore::EventActionSchedule::deserialize(fp);
    fp.close();
}

ReplayScheduler::~ReplayScheduler()
{
    delete m_schedule;
}

void ReplayScheduler::eventActionScheduled(const WebCore::EventActionDescriptor&,
                                           WebCore::EventActionRegister*)
{
}

void ReplayScheduler::executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister)
{

    if (m_schedule->isEmpty()) {
        emit sigDone();
        return;
    }

    WebCore::EventActionDescriptor nextToSchedule = m_schedule->first();

    // try to execute this directly
    bool found = eventActionRegister->runEventAction(nextToSchedule);

    if (!found)
        // Try to do a fuzzy match (only networking at the moment)

        // TODO(WebERA) We could also move the fuzzy matching into EventActionRegister...

        /**
          * Fuzzy match network related event actions
          *
          * With event action A, we will match an event action B iff
          *
          * Handled by fuzzyurl:
          *     A's domain matches B's domain
          *     A's fragments (before ?) matches B's fragments
          *     A's keywords in query (?KEYWORD=VALUE) matches B's keywords
          *
          * Handled here:
          *     A's url sequence number and segment number matches B's ...
          *
          * If we have multiple matches then we use the match score by fuzzyurl.
          *
          * This solves the problem of URLS containing timestamps.
          *
          */
        if (nextToSchedule.getType() == "NETWORK") {

            QString url = QString::fromStdString(nextToSchedule.getParameter(0));
            unsigned int sameUrlSequenceNumber = QString::fromStdString(nextToSchedule.getParameter(1)).toUInt();
            unsigned int sequenceNumber = QString::fromStdString(nextToSchedule.getParameter(2)).toUInt();

            FuzzyUrlMatcher* matcher = new FuzzyUrlMatcher(QUrl(url));

            std::vector<std::string> names = eventActionRegister->getWaitingNames();
            std::vector<std::string>::const_iterator iter;

            unsigned int bestScore = 0;
            WebCore::EventActionDescriptor bestDescriptor;

            for (iter = names.begin(); iter != names.end(); iter++) {

                WebCore::EventActionDescriptor candidate(nextToSchedule.getId(), (*iter), nextToSchedule.getParams());

                if (candidate.getType() != "NETWORK") {
                    continue;
                }

                unsigned int candidateSameUrlSequenceNumber = QString::fromStdString(candidate.getParameter(1)).toUInt();
                unsigned int candidateSequenceNumber = QString::fromStdString(candidate.getParameter(2)).toUInt();

                if (sameUrlSequenceNumber != candidateSameUrlSequenceNumber || sequenceNumber != candidateSequenceNumber) {
                    continue;
                }

                unsigned int score = matcher->score(QUrl(QString::fromStdString(candidate.getParameter(0))));

                if (score > bestScore) {
                    bestScore = score;
                    bestDescriptor = candidate;
                }
            }

            if (bestScore > 0) {
                std::cout << "Fuzzy match of scheduler name, renaming " << nextToSchedule.getName() << " to " << bestDescriptor.getName() << std::endl;
                found = eventActionRegister->runEventAction(bestDescriptor);
            }
    }

    if (found) {
        m_schedule->remove(0);
        m_scheduleWaits = 0;
        return;
    }

    // timer not registered yet

    if (m_scheduleWaits > 500) { // TODO(WebERA) 500 is an arbitrary magic number, reason about a good number
        std::cerr << std::endl << "Error: Failed execution schedule after waiting for 500 iterations..." << std::endl;
        std::cerr << "This is the current queue of events" << std::endl;

        debugPrintTimers(eventActionRegister); // TODO(WebERA): DEBUG

        std::exit(1);
    }

    m_scheduleWaits++;
}

bool ReplayScheduler::isFinished()
{
    return m_schedule->isEmpty();
}

void ReplayScheduler::debugPrintTimers(WebCore::EventActionRegister* eventActionRegister)
{
    std::cout << "=========== TIMERS ===========" << std::endl;
    std::cout << "NEXT -> " << m_schedule->first().getName() << "(" << m_schedule->first().getParams() << ")" << std::endl;
    std::cout << "QUEUE -> " << std::endl;

    eventActionRegister->debugPrintNames();

}
