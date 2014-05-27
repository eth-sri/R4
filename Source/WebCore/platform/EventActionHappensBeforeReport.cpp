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

#include "EventActionHappensBeforeReport.h"

#include <WebCore/platform/ThreadGlobalData.h>
#include <WebCore/platform/ThreadTimers.h>

namespace WebCore {

WTF::EventActionId HBCurrentOrLastEventAction()
{

    if (HBIsCurrentEventActionValid()) {
        return threadGlobalData().threadTimers().happensBefore().currentEventAction();
    } else {
        threadGlobalData().threadTimers().happensBefore().checkValidLastEventAction();
        return threadGlobalData().threadTimers().happensBefore().lastEventAction();
    }

}

WTF::EventActionId HBCurrentEventAction()
{
    threadGlobalData().threadTimers().happensBefore().checkInValidEventAction();
    return threadGlobalData().threadTimers().happensBefore().currentEventAction();
}

WTF::EventActionId HBAllocateEventActionId()
{
    return threadGlobalData().threadTimers().happensBefore().allocateEventActionId();
}

void HBEnterEventAction(WTF::EventActionId id, ActionLog::EventActionType type)
{
    threadGlobalData().threadTimers().happensBefore().setCurrentEventAction(id, type);
}

void HBExitEventAction(bool commit)
{
    threadGlobalData().threadTimers().happensBefore().setCurrentEventActionInvalid(commit);
}

bool HBIsCurrentEventActionValid()
{
    return threadGlobalData().threadTimers().happensBefore().isCurrentEventActionValid();
}

bool HBIsLastUIEventActionValid()
{
    return threadGlobalData().threadTimers().happensBefore().isLastUIEventActionValid();
}

void HBAddExplicitArc(WTF::EventActionId earlier, WTF::EventActionId later)
{
    if (earlier != later) { // filter out self loops
        threadGlobalData().threadTimers().happensBefore().addExplicitArc(earlier, later);
    }
}

void HBAddTimedArc(WTF::EventActionId earlier, WTF::EventActionId later, double duration)
{
    if (earlier != later) { // filter out self loops
        threadGlobalData().threadTimers().happensBefore().addTimedArc(earlier, later, duration);
    }

}

WTF::EventActionId HBLastUIEventAction()
{
    return threadGlobalData().threadTimers().happensBefore().lastUIEventAction();
}

MultiJoinHappensBefore::MultiJoinHappensBefore() : m_joined(false), m_endThreads(NULL) {
}

MultiJoinHappensBefore::~MultiJoinHappensBefore() {
    clear();
}

void MultiJoinHappensBefore::clear() {
    while (m_endThreads != NULL) {
        LList* next = m_endThreads->m_next;
        delete m_endThreads;
        m_endThreads = next;
    }
}

void MultiJoinHappensBefore::threadEndAction() {
    threadGlobalData().threadTimers().happensBefore().checkInValidEventAction();
    // ActionLogFormat(ActionLog::WRITE_MEMORY, "SyncJoin:%p", static_cast<void*>(this));
    m_endThreads = new LList(HBCurrentEventAction(), m_endThreads);
}

void MultiJoinHappensBefore::joinAction() {
    if (m_joined) {
    // It's possible to join multiple threads to one thread.
    //	WTFReportBacktrace();
    }
    m_joined = true;

    if (m_endThreads == NULL) {
        // There are 0 threads to join. Nothing to do.
        return;
    }

    // Note(veselin): We do not allocate a timeslice here, but use the current one (otherwise
    // manipulating the document history causes problems).
    // TimeSliceId joinSlice = threadGlobalData().threadTimers().happensBefore().allocateTimeSlice();
    // threadGlobalData().threadTimers().happensBefore().setCurrentTimeSlice(joinSlice);

    threadGlobalData().threadTimers().happensBefore().checkInValidEventAction();
    WTF::EventActionId joinSlice = threadGlobalData().threadTimers().happensBefore().currentEventAction();
    LList* curr = m_endThreads;
    while (curr != NULL) {
        if (curr->m_id != joinSlice) {
            threadGlobalData().threadTimers().happensBefore().addExplicitArc(
                    curr->m_id, joinSlice);
        }
        curr = curr->m_next;
    }

    // ActionLogFormat(ActionLog::READ_MEMORY, "SyncJoin:%p", static_cast<void*>(this));
}

HBDisabledInstrumentation::HBDisabledInstrumentation() {
    threadGlobalData().threadTimers().happensBefore().addDisableInstrumentationRequest();
}

HBDisabledInstrumentation::~HBDisabledInstrumentation() {
    threadGlobalData().threadTimers().happensBefore().removeDisableInstrumentationRequest();
}

}
