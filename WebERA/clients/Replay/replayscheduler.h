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

#include <string>

#include <wtf/ExportMacros.h>
#include <wtf/Vector.h>

#include <WebCore/platform/Timer.h>
#include <WebCore/platform/schedule/Scheduler.h>

#include "eventaction/EventActionSchedule.h"
#include "eventaction/EventActionDescriptor.h"

#ifndef REPLAYSCHEDULER_H
#define REPLAYSCHEDULER_H

class ReplayScheduler : public WebCore::Scheduler
{
public:
    ReplayScheduler(const std::string& schedulePath);
    ~ReplayScheduler();

    int selectNextSchedulableItem(const WTF::Vector<WebCore::TimerBase*>& items);

    bool isFinished();

private:

    void debugPrintTimers(const WTF::Vector<WebCore::TimerBase*>& items);

    WTF::Vector<WebCore::EventActionDescriptor> m_schedule;

    unsigned int m_scheduleWaits;
};

#endif // REPLAYSCHEDULER_H