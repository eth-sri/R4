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

#include <iostream>
#include <sstream>
#include "config.h"
#include "ScriptRunner.h"

#include "CachedScript.h"
#include "Document.h"
#include "Element.h"
#include "PendingScript.h"
#include "ScriptElement.h"

#include <wtf/EventActionDescriptor.h>
#include <wtf/ActionLogReport.h>

namespace WebCore {

DeferAsyncScriptExecution::DeferAsyncScriptExecution(const PendingScript& script, Document *document, unsigned int scriptRunnerId, unsigned int scriptOffset)
    : m_timer(this, &DeferAsyncScriptExecution::timerFired)
    , m_script(script)
    , m_document(document)
    , m_suspended(false)
{

    std::stringstream params;
    params << scriptRunnerId << ",";
    params << "Async" << ",";
    params << scriptOffset;

    WTF::EventActionDescriptor descriptor(WTF::PARSING, "ScriptRunner", params.str());

    m_timer.setEventActionDescriptor(descriptor);
    m_timer.startOneShot(0);
}

DeferAsyncScriptExecution::~DeferAsyncScriptExecution()
{
    if (m_timer.isActive() || m_suspended) {
        m_timer.stop();
        m_document->decrementLoadEventDelayCount();
    }
}

void DeferAsyncScriptExecution::timerFired(Timer<DeferAsyncScriptExecution> *timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);

    RefPtr<Document> protect(m_document);

    CachedScript* cachedScript = m_script.cachedScript();
    RefPtr<Element> element = m_script.releaseElementAndClear();
    ScriptElement* script = toScriptElement(element.get());
    ActionLogFormat(ActionLog::READ_MEMORY, "ScriptRunner-%p-%p",
            static_cast<void*>(this), static_cast<void*>(script));
    script->execute(cachedScript);
    m_document->decrementLoadEventDelayCount();

    // No need to add HB relation for the event action loading the resource. That relation should have been
    // added implicitly when creating the timer.

    // HB resume join

    m_resumeJoin.joinAction();
    m_resumeJoin.clear();
}

void DeferAsyncScriptExecution::suspend()
{
    if (m_timer.isActive()) {
        m_suspended = true;
        m_timer.stop();
    }
}

void DeferAsyncScriptExecution::resume()
{
    if (m_suspended) {
        m_suspended = false;
        m_timer.startOneShot(0);
    }

    m_resumeJoin.threadEndAction();
}

unsigned int ScriptRunner::m_seqNumber = 0;

ScriptRunner::ScriptRunner(Document* document)
    : m_document(document)
    , m_pendingScriptsAdded(0)
    , m_scriptRunnerId(ScriptRunner::getSeqNumber())
    , m_inOrderTimer(this, &ScriptRunner::inOrderTimerFired)
    , m_inOrderExecuted(0)
    , m_inOrderLastEventAction(0)
    , m_suspended(false)
{
    ASSERT(document);
}

ScriptRunner::~ScriptRunner()
{
    for (size_t i = 0; i < m_scriptsToExecuteSoon.size(); ++i)
        m_document->decrementLoadEventDelayCount();
    for (size_t i = 0; i < m_scriptsToExecuteInOrder.size(); ++i)
        m_document->decrementLoadEventDelayCount();
    for (int i = 0; i < m_pendingAsyncScripts.size(); ++i)
        m_document->decrementLoadEventDelayCount();

    for (size_t i = 0; i < m_pendingAsyncScriptsExecuted.size(); ++i) {
        delete m_pendingAsyncScriptsExecuted[i]; // decrements the counter
    }
}

void ScriptRunner::queueScriptForExecution(ScriptElement* scriptElement, CachedResourceHandle<CachedScript> cachedScript, ExecutionType executionType)
{
    ASSERT(scriptElement);
    ASSERT(cachedScript.get());

    Element* element = scriptElement->element();
    ASSERT(element);
    ASSERT(element->inDocument());

    m_document->incrementLoadEventDelayCount();

    switch (executionType) {
    case ASYNC_EXECUTION:
        m_pendingAsyncScripts.add(scriptElement, std::pair<unsigned int, PendingScript>(m_pendingScriptsAdded++, PendingScript(element, cachedScript.get())));
        break;

    case IN_ORDER_EXECUTION:
        m_scriptsToExecuteInOrder.append(PendingScript(element, cachedScript.get()));
        ActionLogFormat(ActionLog::WRITE_MEMORY, "ScriptRunner-%p-%p",
        		static_cast<void*>(this), static_cast<void*>(scriptElement));
        break;
    }
}

void ScriptRunner::suspend()
{
    m_suspendJoin.threadEndAction();

    if (m_inOrderTimer.isActive()) {
        m_suspended = true;
        m_inOrderTimer.stop();
    }
}

void ScriptRunner::resume()
{
    m_suspendJoin.joinAction();
    m_suspendJoin.clear();

    if (m_suspended) {
        m_suspended = false;
        m_inOrderTimer.startOneShot(0);
    }

    m_resumeJoin.threadEndAction();

}

void ScriptRunner::notifyScriptReady(ScriptElement* scriptElement, ExecutionType executionType)
{
    ActionLogFormat(ActionLog::WRITE_MEMORY, "ScriptRunner-%p-%p",
            static_cast<void*>(this), static_cast<void*>(scriptElement));

    std::pair<unsigned int, PendingScript> elm;

    switch (executionType) {
    case ASYNC_EXECUTION:
        ASSERT(m_pendingAsyncScripts.contains(scriptElement));

        elm = m_pendingAsyncScripts.take(scriptElement);
        m_pendingAsyncScriptsExecuted.append(new DeferAsyncScriptExecution(elm.second, m_document, m_scriptRunnerId, elm.first));

        return;

    case IN_ORDER_EXECUTION:
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());

        updateInOrderTimer();

        return;
    }

    ASSERT_NOT_REACHED();
}

void ScriptRunner::updateInOrderTimer() {

    if (m_scriptsToExecuteInOrder.size() == 0 || !m_scriptsToExecuteInOrder[0].cachedScript()->isLoaded()) {
        return;
    }

    if (m_inOrderTimer.isActive()) {
        return;
    }

    std::stringstream params;
    params << m_scriptRunnerId << ",";
    params << "InOrder" << ",";
    params << m_inOrderExecuted;

    WTF::EventActionDescriptor descriptor(WTF::PARSING, "ScriptRunner", params.str());

    m_inOrderTimer.setEventActionDescriptor(descriptor);
    m_inOrderTimer.startOneShot(0);
}

void ScriptRunner::inOrderTimerFired(Timer<ScriptRunner>* timer)
{
    m_inOrderExecuted++;

    ASSERT_UNUSED(timer, timer == &m_inOrderTimer);

    RefPtr<Document> protect(m_document);

    PendingScript pendingScript = m_scriptsToExecuteInOrder[0];
    m_scriptsToExecuteInOrder.remove(0);

    // Execute the script

    CachedScript* cachedScript = pendingScript.cachedScript();
    RefPtr<Element> element = pendingScript.releaseElementAndClear();
    ScriptElement* script = toScriptElement(element.get());
    ActionLogFormat(ActionLog::READ_MEMORY, "ScriptRunner-%p-%p",
            static_cast<void*>(this), static_cast<void*>(script));
    script->execute(cachedScript);
    m_document->decrementLoadEventDelayCount();

    // Happens before - chaining

    if (m_inOrderLastEventAction != 0) {
        HBAddExplicitArc(m_inOrderLastEventAction, HBCurrentEventAction());
    }

    m_inOrderLastEventAction = HBCurrentEventAction();

    // Happens before - resource load

    ASSERT(script->eventActionToFinish() != 0);
    HBAddExplicitArc(script->eventActionToFinish(), HBCurrentEventAction());

    // Happens before - resume join

    m_resumeJoin.joinAction();
    m_resumeJoin.clear();

    // Schedule the next script if possible

    updateInOrderTimer();
}

}
