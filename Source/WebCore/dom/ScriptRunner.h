/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ScriptRunner_h
#define ScriptRunner_h

#include <utility>

#include "CachedResourceHandle.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/EventActionDescriptor.h>

#include "WebCore/dom/PendingScript.h"

namespace WebCore {

class CachedScript;
class Document;
class PendingScript;
class ScriptElement;

/** WebERA:
 *
 * This is used to split all async scripts into individual event actions. Regardless of when the scripts are scheduled for execution, they
 * will be added deferred and executed using one of these objects.
 *
 * The "normal" execution logic (with one internal timer) is changed to only handle deferred scripts. Deferred scripts are executed in-order.
 * Only one deferred script will be released to the scheduler at any time.
 *
 * Async scripts are given to the scheduler immeditately using a DeferAsyncScriptExecution.
 *
 */
class DeferAsyncScriptExecution {

    public:
        DeferAsyncScriptExecution(const PendingScript& script, Document* document, unsigned int scriptRunnerId, unsigned int scriptOffset);
        ~DeferAsyncScriptExecution();

    private:

        void timerFired(Timer<DeferAsyncScriptExecution>* timer);
        Timer<DeferAsyncScriptExecution> m_timer;

        PendingScript m_script;
        Document* m_document;

};

class ScriptRunner {
    WTF_MAKE_NONCOPYABLE(ScriptRunner); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ScriptRunner> create(Document* document) { return adoptPtr(new ScriptRunner(document)); }
    ~ScriptRunner();

    enum ExecutionType { ASYNC_EXECUTION, IN_ORDER_EXECUTION };
    void queueScriptForExecution(ScriptElement*, CachedResourceHandle<CachedScript>, ExecutionType);
    bool hasPendingScripts() const { return !m_scriptsToExecuteSoon.isEmpty() || !m_scriptsToExecuteInOrder.isEmpty() || !m_pendingAsyncScripts.isEmpty(); }
    void suspend();
    void resume();
    void notifyScriptReady(ScriptElement*, ExecutionType);

private:
    ScriptRunner(Document*);

    void inOrderTimerFired(Timer<ScriptRunner>*);
    void updateInOrderTimer();

    Document* m_document;
    Vector<PendingScript> m_scriptsToExecuteInOrder;
    Vector<PendingScript> m_scriptsToExecuteSoon; // http://www.whatwg.org/specs/web-apps/current-work/#set-of-scripts-that-will-execute-as-soon-as-possible
    HashMap<ScriptElement*, std::pair<unsigned int, PendingScript> > m_pendingAsyncScripts;
    unsigned int m_pendingScriptsAdded;
    Vector<DeferAsyncScriptExecution*> m_pendingAsyncScriptsExecuted;

    static unsigned int getSeqNumber() {
        return ScriptRunner::m_seqNumber++;
    }
    static unsigned int m_seqNumber;

    unsigned int m_scriptRunnerId;

    Timer<ScriptRunner> m_inOrderTimer;
    unsigned int m_inOrderExecuted;
    EventActionId m_inOrderLastEventAction;

};

}

#endif
