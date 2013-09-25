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

#include <wtf/Noncopyable.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/StringSet.h>
#include <fstream>

namespace WebCore {

    class SharedTimer;
    class TimerBase;

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

        // A list of names of timers.
        StringSet* timerNames() { return &m_timerNames; }

        // The index of the name (index in timerNames()) of the timer that currently executes.
        // The value -2 means that a timer is currently not executed. A value -1 means that an
        // unnamed timer executes.
        int getCurrentTimerNameIndex() { return m_currentTimerNameIndex; }
        void setCurrentTimerNameIndex(int currentTimerNameIndex) {
        	m_currentTimerNameIndex = currentTimerNameIndex;
        }

        void updateSharedTimer();
        void fireTimersInNestedEventLoop();

    private:
        static void sharedTimerFired();

        void sharedTimerFiredInternal();
        void fireTimersInNestedEventLoopInternal();

        std::string currentScheduledEvent();
        void nextScheduledEvent();
        bool inReplayMode();
        TimerBase* getTimerForNextEvent();
        void debugPrintTimers();

        Vector<TimerBase*> m_timerHeap;
        StringSet m_timerNames;
        int m_currentTimerNameIndex;
        SharedTimer* m_sharedTimer; // External object, can be a run loop on a worker thread. Normally set/reset by worker thread.
        bool m_firingTimers; // Reentrancy guard.

        std::ifstream m_schedule;
        std::string m_nextEventName;
        uint m_scheduleWaits;
    };

}

#endif
