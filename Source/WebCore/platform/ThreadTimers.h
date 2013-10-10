/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#ifndef ThreadTimers_h
#define ThreadTimers_h

#include <wtf/ExportMacros.h>
#include <wtf/Noncopyable.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/StringSet.h>

#include <platform/schedule/Scheduler.h>

#include "schedule/EventActionRegister.h"
#include "eventaction/EventActionSchedule.h"
#include "eventaction/EventActionHB.h"

namespace WebCore {

    class SharedTimer;
    class TimerBase;
    class EventActionRegister;

    /**
      *
      * WebERA:
      *
      * This is a modified ThreadTimers for demoing simple record-and-replay of executions.
      *
      * It has two modes, a record mode and a replay mode.
      *
      * If the file /tmp/input exists on the system it will automatically go into replay mode.
      * This file should contain the output otherwise emitted by the record mode (a schedule consisting of a list of events)
      *
      * In record mode it will use the schedule to decide which of the pending events should be scheduled next.
      * If it can't find an appropiate event then it will wait for some "magic" amount of time and eventually error out.
      *
      */

    // A collection of timers per thread. Kept in ThreadGlobalData.
    class ThreadTimers {
        WTF_MAKE_NONCOPYABLE(ThreadTimers); WTF_MAKE_FAST_ALLOCATED;
    public:
        ThreadTimers();
        ~ThreadTimers();

        // On a thread different then main, we should set the thread's instance of the SharedTimer.
        void setSharedTimer(SharedTimer*);

        Vector<TimerBase*>& timerHeap() { return m_timerHeap; }

        void updateSharedTimer();
        void fireTimersInNestedEventLoop();

        // WebERA:
        // These are static because we can't (for some unknown reason) access these through threadglobaldata
        // from the clients. If we do that we will get an invalid pointer back.
        // Making these static is not correct since multiple threads will be merged together, however we do not
        // support multiple threads either way (nor workers) so this should not result in any problems in practice.
        EventActionRegister* eventActionRegister() { return &m_eventActionRegister; }
        EventActionsHB* eventActionsHB() { return &m_eventActionsHB; }

        // Only the scheduler can be static. All the other objects are thread-local.
        static void setScheduler(Scheduler* scheduler);

    private:
        static void sharedTimerFired();

        static bool fireTimerCallback(void* object, const std::string& params);

        void sharedTimerFiredInternal();
        void fireTimersInNestedEventLoopInternal();

        Vector<TimerBase*> m_timerHeap;
        SharedTimer* m_sharedTimer; // External object, can be a run loop on a worker thread. Normally set/reset by worker thread.
        bool m_firingTimers; // Reentrancy guard.

        // WebERA

        // Only the scheduler is static. The other WebERA objects must be thread-local.
        static Scheduler* m_scheduler;

        // Thread-local.
        EventActionRegister m_eventActionRegister;
        EventActionsHB m_eventActionsHB;
    };

}

#endif
