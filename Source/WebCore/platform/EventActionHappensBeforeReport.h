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
 */

#ifndef EVENTACTIONHAPPENSBEFOREREPORT_H
#define EVENTACTIONHAPPENSBEFOREREPORT_H

#include <wtf/ExportMacros.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>
#include <wtf/ActionLog.h>

#include <wtf/EventActionDescriptor.h>

namespace WebCore {

EventActionId HBCurrentEventAction();
EventActionId HBAllocateEventActionId();

void HBEnterEventAction(EventActionId id, ActionLog::EventActionType type);
void HBExitEventAction();

bool HBIsCurrentEventActionValid();

// ONLY ADD ARCS BETWEEN EVENT ACTIONS THAT HAVE BEEN ENTERED!
void HBAddExplicitArc(EventActionId earlier, EventActionId later);
void HBAddTimedArc(EventActionId earlier, EventActionId later, double duration);

// AUTOMATICALLY ADD ARCS BETWEEN AN EVENT ACTION AND THE EVENT ACTION OF A TIMER (IF IT IS TRIGGERED)
/*void HBConditionalArc(EventActionId earlier, void* later);
void HBConditionalTimedArc(EventActionId earlier, void* later, double duration);
void HBAddConditionalArcs(void* later);*/

EventActionId HBLastUIEventAction();

// Class to instrument ad-hoc synchronization in WebKit and obtain happens-before.
class MultiJoinHappensBefore {
    WTF_MAKE_NONCOPYABLE(MultiJoinHappensBefore); WTF_MAKE_FAST_ALLOCATED;
public:
    MultiJoinHappensBefore();
    ~MultiJoinHappensBefore();

    // Must be called from every event action that sets an internal WebKit
    // ad-hoc synchronization variable.
    void threadEndAction();

    // Must be called from the internal WebKit ad-hoc that waits for all
    // threadEndAction(s) to finish.
    void joinAction();

    void clear();

private:
    struct LList {
        LList(int id, LList* next) : m_id(id), m_next(next) {}
        EventActionId m_id;
        LList* m_next;
    };

    bool m_joined;
    LList* m_endThreads;
};

class HBDisabledInstrumentation {
public:
    HBDisabledInstrumentation();
    ~HBDisabledInstrumentation();
};

}

#endif // EVENTACTIONHAPPENSBEFOREREPORT_H
