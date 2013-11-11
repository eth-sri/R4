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


#include "EventActionHappensBefore.h"

#include "config.h"

#include <wtf/ActionLogReport.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>
#include <stdio.h>
#include <iostream>

namespace WebCore {

EventActionsHB::EventActionsHB()
    : m_currentEventActionId(0) // 0 is reserved as invalid event action
    , m_nextEventActionId(1) // 0 is reserved as an empty value in e.g. hashtables
    , m_lastUIEventAction(0)
    , m_numDisabledInstrumentationRequests(0)
{
}

EventActionsHB::~EventActionsHB() {
}

EventActionId EventActionsHB::allocateEventActionId() {
    return m_nextEventActionId++;
}

void EventActionsHB::addExplicitArc(EventActionId earlier, EventActionId later) {
    if (earlier <= 0 || later <= 0 || earlier == later) {
        CRASH();
    }
    ActionLogAddArc(earlier, later, -1);
}

void EventActionsHB::addTimedArc(EventActionId earlier, EventActionId later, double duration) {
    if (earlier <= 0 || later <= 0) {
        CRASH();
    }
    ActionLogAddArc(earlier, later, duration * 1000);
}

//void EventActionsHB::addConditionalArc(EventActionId earlier, void* later) {
    /*if (m_currentEventActionId == -1) return;

        PendingArcVector& pending_arc_vector = m_pendingArcs[reinterpret_cast<long int>(eventId)];
        pending_arc_vector.push_back(PendingArc(m_currentEventActionId, 0));
    }*/
//}

//void EventActionsHB::addConditionalTimedArc(EventActionId earlier, void* later, double duration) {

//}

//void EventActionsHB::addConditionalArcs(void* later) {
    /*if (m_currentEventActionId == -1) return;

    PendingArcs::iterator it = m_pendingArcs.find(reinterpret_cast<long int>(eventId));
    if (it == m_pendingArcs.end()) return;

    PendingArcVector& pending_arc_vector = it->second;

    for (PendingArcVector::iterator iter = pending_arc_vector.begin(); iter != pending_arc_vector.end(); iter++) {
        if (m_currentEventActionId <= iter->m_eventActionId) {
            CRASH();
        }
        addArc(iter->m_eventActionId, m_currentEventActionId, iter->m_timeout);
    }

    m_pendingArcs.erase(it);*/
//}

void EventActionsHB::setCurrentEventAction(EventActionId newEventActionId, ActionLog::EventActionType type) {
    m_currentEventActionId = newEventActionId;

    ActionLogEnterOperation(newEventActionId, type);

    if (type == ActionLog::USER_INTERFACE) {
        if (m_lastUIEventAction != 0) {

            addExplicitArc(m_lastUIEventAction, newEventActionId);
            m_lastUIEventAction = newEventActionId;
        }
    }
}

void EventActionsHB::setCurrentEventActionInvalid() {
    ActionLogExitOperation();
    m_currentEventActionId = 0;
}

void EventActionsHB::checkInValidEventAction() {
    if (m_currentEventActionId != 0) {
        fprintf(stderr, "Not in a valid event action.\n");
        fflush(stderr);
        CRASH();
    }
}


}

