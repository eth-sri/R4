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
    : QObject(NULL)
    , Scheduler()
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

void ReplayScheduler::eventActionScheduled(const WebCore::EventActionDescriptor& descriptor,
                                           WebCore::EventActionRegister& eventActionRegister)
{
}

void ReplayScheduler::executeDelayedEventActions(WebCore::EventActionRegister& eventActionRegister)
{

    if (m_schedule->isEmpty()) {
        emit sigDone();
        return;
    }

    WebCore::EventActionDescriptor nextToSchedule = m_schedule->first();

    bool found = eventActionRegister.runEventAction(nextToSchedule);

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

void ReplayScheduler::debugPrintTimers(WebCore::EventActionRegister& eventActionRegister)
{
    std::cout << "=========== TIMERS ===========" << std::endl;
    std::cout << "NEXT -> " << m_schedule->first().getName() << "(" << m_schedule->first().getParams() << ")" << std::endl;
    std::cout << "QUEUE -> " << std::endl;

    eventActionRegister.debugPrintNames();

}
