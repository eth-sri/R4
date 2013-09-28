/*
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
 *
 */

#include <iostream>

#include <WebCore/platform/ThreadGlobalData.h>
#include <WebCore/platform/ThreadTimers.h>

#include "RepeatScheduler.h"

namespace WebCore {

RepeatScheduler::RepeatScheduler()
    : m_scheduleWaits(0)
{

    m_schedule.open("/tmp/input");
    updateNextTimerToSchedule(); // read the first event in the schedule

}

RepeatScheduler::~RepeatScheduler()
{
    if (m_schedule.is_open()) {
        m_schedule.close();
    }

}

int RepeatScheduler::selectNextSchedulableItem(const WTF::Vector<TimerBase*>& items)
{
    // Search for the next timer and return its index
    for(Vector<TimerBase*>::const_iterator it = items.begin(); it != items.end(); ++it) {
        if ((*it)->eventActionDescriptor().getDescriptionIndex() == m_nextTimerToScheduleInt) {

            debugPrintTimers(items); // TODO(WebERA): DEBUG

            updateNextTimerToSchedule();

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

bool RepeatScheduler::isFinished()
{
    return !m_schedule.is_open() || !m_schedule.good();
}

void RepeatScheduler::updateNextTimerToSchedule()
{
    if (!isFinished()) {
        getline(m_schedule, m_nextTimerToSchedule);

    } else {
        m_nextTimerToSchedule = "__EOS__";
    }

    m_nextTimerToScheduleInt = EventActionDescriptor::descriptions()->addString(m_nextTimerToSchedule.c_str());
}

void RepeatScheduler::debugPrintTimers(const WTF::Vector<TimerBase*>& items)
{
    std::cout << "=========== TIMERS ===========" << std::endl;
    std::cout << "NEXT -> (" << m_nextTimerToScheduleInt << ") " << m_nextTimerToSchedule << std::endl << std::endl;

    for(Vector<TimerBase*>::const_iterator it = items.begin(); it != items.end(); ++it) {
        std::string eventName = (*it)->eventActionDescriptor().getDescription();
        std::cout << "(" << (*it)->eventActionDescriptor().getDescriptionIndex() << ") " << eventName << std::endl;
    }

}

}
