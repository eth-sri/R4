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


#ifndef EVENTACTIONHAPPENSBEFORE_H
#define EVENTACTIONHAPPENSBEFORE_H

#include <wtf/ExportMacros.h>
#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <wtf/StringSet.h>
#include <wtf/EventActionSchedule.h>
#include <wtf/EventActionDescriptor.h>
#include <wtf/ActionLog.h>

namespace WebCore {

// Happens before graph for event actions.
class EventActionsHB {
    WTF_MAKE_NONCOPYABLE(EventActionsHB); WTF_MAKE_FAST_ALLOCATED;
public:
    EventActionsHB();
    ~EventActionsHB();

    // Allocates a new id for an event action.
    EventActionId allocateEventActionId();

    EventActionId currentEventAction() const {
        if (m_currentEventActionId == 0) {
            CRASH();
        }
        return m_currentEventActionId;
    }

    void setCurrentEventAction(EventActionId newId, ActionLog::EventActionType type);
    void setCurrentEventActionInvalid();

    bool isCurrentEventActionValid() const {
        return m_currentEventActionId != 0;
    }

    void addExplicitArc(EventActionId earlier, EventActionId later);
    void addTimedArc(EventActionId earlier, EventActionId later, double duration);

    EventActionId lastUIEventAction() const {
         return m_lastUIEventAction;
    }

    void checkInValidEventAction();

    void addDisableInstrumentationRequest() {
        ++m_numDisabledInstrumentationRequests;
    }

    void removeDisableInstrumentationRequest() {
        --m_numDisabledInstrumentationRequests;
    }

    bool isInstrumentationDisabled() const {
        return m_numDisabledInstrumentationRequests > 0;
    }

private:
    EventActionId m_currentEventActionId;
    EventActionId m_nextEventActionId;

    EventActionId m_lastUIEventAction;

    int m_numDisabledInstrumentationRequests;
};


}


#endif // EVENTACTIONHAPPENSBEFORE_H
