/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef Timer_h
#define Timer_h

#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>
#include <wtf/EventActionDescriptor.h>

#include "EventActionHappensBeforeReport.h"

namespace WebCore {

// Time intervals are all in seconds.

class TimerHeapElement;

class TimerBase {
    WTF_MAKE_NONCOPYABLE(TimerBase); WTF_MAKE_FAST_ALLOCATED;
public:
    TimerBase();
    virtual ~TimerBase();

    void ignoreFireIntervalForHappensBefore() {
    	m_ignoreFireIntervalForHappensBefore = true;
    }

    void start(double nextFireInterval, double repeatInterval);

    // TODO(WebERA): Repeating timers should be split up into multiple descriptors or?
    void startRepeating(double repeatInterval) { start(repeatInterval, repeatInterval); }
    void startOneShot(double interval) { start(interval, 0); }

    void stop();
    bool isActive() const;

    double nextFireInterval() const;
    double repeatInterval() const { return m_repeatInterval; }

    void augmentFireInterval(double delta) { setNextFireTime(m_nextFireTime + delta, delta); }
    void augmentRepeatInterval(double delta) { augmentFireInterval(delta); m_repeatInterval += delta; }

    static void fireTimersInNestedEventLoop();

    // WebERA: Mark this timer as a representative for an event action.
    // The WebERA system may delay a tiemr depending on the event action.
    // Note: once a timer fires, changing the descriptor will only affect future timer fires.
    void setEventActionDescriptor(const EventActionDescriptor& descriptor) { m_eventActionDescriptor = descriptor; }
    const EventActionDescriptor& eventActionDescriptor() const { return m_eventActionDescriptor; }

    // WebERA: This is used to overwrite the active bit even though the timer has been pulled out of the timer
    // heap. This is used as part of the event action registry
    void inEventActionRegister(bool active) {
        m_inEventActionRegister = active;
    }

private:
    virtual void fired() = 0;

    void checkConsistency() const;
    void checkHeapIndex() const;

    void setNextFireTime(double, double);

    bool inHeap() const { return m_heapIndex != -1; }

    void heapDecreaseKey();
    void heapDelete();
    void heapDeleteMin();
    void heapIncreaseKey();
    void heapInsert();
    void heapPop();
    void heapPopMin();

    double m_nextFireTime; // 0 if inactive
    double m_repeatInterval; // 0 if not repeating
    int m_heapIndex; // -1 if not in heap
    unsigned m_heapInsertionOrder; // Used to keep order among equal-fire-time timers

    EventActionId m_lastFireEventAction;
    EventActionId m_starterEventAction;

private:
    double m_nextFireInterval;  // The interval for which the next file was set to.
    bool m_ignoreFireIntervalForHappensBefore;

    EventActionDescriptor m_eventActionDescriptor;
    bool m_inEventActionRegister;

#ifndef NDEBUG
    ThreadIdentifier m_thread;
#endif

    friend class ThreadTimers;
    friend class TimerHeapLessThanFunction;
    friend class TimerHeapReference;
};

template <typename TimerFiredClass> class Timer : public TimerBase {
public:
    typedef void (TimerFiredClass::*TimerFiredFunction)(Timer*);

    Timer(TimerFiredClass* o, TimerFiredFunction f)
        : m_object(o), m_function(f) { }

private:
    virtual void fired() { (m_object->*m_function)(this); }

    TimerFiredClass* m_object;
    TimerFiredFunction m_function;
};

inline bool TimerBase::isActive() const
{
    ASSERT(m_thread == currentThread());
    return m_nextFireTime || m_inEventActionRegister;
}

}

#endif
