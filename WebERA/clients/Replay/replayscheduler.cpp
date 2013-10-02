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

#include "replayscheduler.h"

ReplayScheduler::ReplayScheduler(const std::string& schedulePath)
    : Scheduler()
{
    std::ifstream fp;
    fp.open(schedulePath);
    WebCore::EventActionSchedule* schedule = WebCore::EventActionSchedule::deserialize(fp);
    m_schedule = schedule->getVectorCopy();
    delete schedule;
    fp.close();
}

ReplayScheduler::~ReplayScheduler()
{
}

int ReplayScheduler::selectNextSchedulableItem(const WTF::Vector<WebCore::TimerBase*>& items)
{

    WebCore::EventActionDescriptor nextToSchedule = m_schedule.first();

    // Search for the next timer and return its index
    for(WTF::Vector<WebCore::TimerBase*>::const_iterator it = items.begin(); it != items.end(); ++it) {
        if ((*it)->eventActionDescriptor().compareDescription(nextToSchedule) ||
                ((*it)->eventActionDescriptor().isNull() && nextToSchedule.isNull())) {

            std::cout << "DISPATCH " << nextToSchedule.getDescription() << std::endl;
            m_schedule.remove(0);

            m_scheduleWaits = 0;
            return it - items.begin();
        }
    }

    // timer not registered yet

    if (m_scheduleWaits > 500) { // TODO(WebERA) 500 is an arbitrary magic number, reason about a good number
        std::cerr << std::endl << "Error: Failed execution schedule after waiting for 500 iterations..." << std::endl;
        std::cerr << "This is the current queue of events" << std::endl;

        debugPrintTimers(items); // TODO(WebERA): DEBUG

        std::exit(1);
    }

    m_scheduleWaits++;

    return Scheduler::YIELD;

}

bool ReplayScheduler::isFinished()
{
    return m_schedule.isEmpty();
}

void ReplayScheduler::debugPrintTimers(const WTF::Vector<WebCore::TimerBase*>& items)
{
    std::cout << "=========== TIMERS ===========" << std::endl;
    std::cout << "NEXT -> " << m_schedule.first().getDescription() << std::endl << std::endl;
    std::cout << "QUEUE -> " << std::endl;

    for(Vector<WebCore::TimerBase*>::const_iterator it = items.begin(); it != items.end(); ++it) {
        std::cout << "| " << (*it)->eventActionDescriptor().getDescription() << std::endl;
    }

}
