/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 */

#include <sstream>
#include "config.h"
#include "DocumentEventQueue.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptExecutionContext.h"
#include "SuspendableTimer.h"

#include <WebCore/platform/EventActionHappensBeforeReport.h>
#include <wtf/ActionLogReport.h>

namespace WebCore {
    
static inline bool shouldDispatchScrollEventSynchronously(Document* document)
{
    ASSERT_ARG(document, document);
    return applicationIsSafari() && (document->url().protocolIs("feed") || document->url().protocolIs("feeds"));
}

class DocumentEventQueueTimer : public SuspendableTimer {
    WTF_MAKE_NONCOPYABLE(DocumentEventQueueTimer);
public:
    DocumentEventQueueTimer(DocumentEventQueue* eventQueue, ScriptExecutionContext* context)
        : SuspendableTimer(context)
        , m_eventQueue(eventQueue) { }

private:
    virtual void fired() { m_eventQueue->pendingEventTimerFired(); }
    DocumentEventQueue* m_eventQueue;
};

PassRefPtr<DocumentEventQueue> DocumentEventQueue::create(ScriptExecutionContext* context)
{
    return adoptRef(new DocumentEventQueue(context));
}

DocumentEventQueue::DocumentEventQueue(ScriptExecutionContext* context)
    : m_pendingEventTimer(adoptPtr(new DocumentEventQueueTimer(this, context)))
    , m_isClosed(false)
    , m_lastFireEventAction(0)
{
    m_pendingEventTimer->suspendIfNeeded();
}

DocumentEventQueue::~DocumentEventQueue()
{
}

bool DocumentEventQueue::enqueueEvent(PassRefPtr<Event> event)
{
    if (m_isClosed)
        return false;

    ASSERT(event->target());
    bool wasAdded = m_queuedEvents.add(event).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    if (HBIsCurrentEventActionValid()) {
        m_queuedSources.push_back(HBCurrentEventAction());
    } else { // A repaint can cause a resize event, associate this event with the last UI action
        m_queuedSources.push_back(HBLastUIEventAction());
    }
    
    tryUpdateAndStartTimer();

    return true;
}

void DocumentEventQueue::enqueueOrDispatchScrollEvent(PassRefPtr<Node> target, ScrollEventTargetType targetType)
{
    if (!target->document()->hasListenerType(Document::SCROLL_LISTENER))
        return;

    // Per the W3C CSSOM View Module, scroll events fired at the document should bubble, others should not.
    bool canBubble = targetType == ScrollEventDocumentTarget;
    RefPtr<Event> scrollEvent = Event::create(eventNames().scrollEvent, canBubble, false /* non cancelleable */);
     
    if (shouldDispatchScrollEventSynchronously(target->document())) {
        target->dispatchEvent(scrollEvent.release());
        return;
    }

    if (!m_nodesWithQueuedScrollEvents.add(target.get()).isNewEntry)
        return;

    scrollEvent->setTarget(target);
    enqueueEvent(scrollEvent.release());
}

bool DocumentEventQueue::cancelEvent(Event* event)
{
    bool found = m_queuedEvents.contains(event);

    // WebERA: Remove the source from the queue
    unsigned int index = 0;
    ListHashSet<RefPtr<Event> >::const_iterator it = m_queuedEvents.begin();
    std::list<EventActionId>::iterator it2 = m_queuedSources.begin();
    while ((*it) != event) {
        index++;
        ++it;
        it2++;
    }
    m_queuedSources.erase(it2);

    m_queuedEvents.remove(event);
    if (m_queuedEvents.isEmpty())
        m_pendingEventTimer->stop();
    return found;
}

void DocumentEventQueue::close()
{
    m_isClosed = true;
    m_pendingEventTimer->stop();
    m_queuedEvents.clear();
}

unsigned int DocumentEventQueue::m_seqNumber = 0;

void DocumentEventQueue::tryUpdateAndStartTimer()
{
    if (!m_pendingEventTimer->isActive()) {

        std::stringstream params;
        params << DocumentEventQueue::getSeqNumber();

        EventActionDescriptor descriptor("DocumentEventQueue", params.str());

        m_pendingEventTimer->setEventActionDescriptor(descriptor);
        m_pendingEventTimer->startOneShot(0);
    }
}

void DocumentEventQueue::pendingEventTimerFired()
{
    ASSERT(!m_pendingEventTimer->isActive());
    ASSERT(!m_queuedEvents.isEmpty());

    m_nodesWithQueuedScrollEvents.clear();

    /*
    // Insert a marker for where we should stop.
    ASSERT(!m_queuedEvents.contains(0));
    bool wasAdded = m_queuedEvents.add(0).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    RefPtr<DocumentEventQueue> protector(this);

    while (!m_queuedEvents.isEmpty()) {
        ListHashSet<RefPtr<Event> >::iterator iter = m_queuedEvents.begin();
        RefPtr<Event> event = *iter;
        m_queuedEvents.remove(iter);
        if (!event)
            break;
        dispatchEvent(event.get());
    }
    */

    // WebERA: Only trigger one event action at a  time

    RefPtr<DocumentEventQueue> protector(this);

    RefPtr<Event> event = m_queuedEvents.first();
    m_queuedEvents.remove(0);

    if (event) {
        dispatchEvent(event.get());
    }

    // WebERA: Happens before (triggering event)
    HBAddExplicitArc(m_queuedSources.front(), HBCurrentEventAction());
    m_queuedSources.pop_front();

    // WebERA: Happens before (chaining)
    if (m_lastFireEventAction != 0) {
        HBAddExplicitArc(m_lastFireEventAction, HBCurrentEventAction());
    }

    m_lastFireEventAction = HBCurrentEventAction();
}

void DocumentEventQueue::dispatchEvent(PassRefPtr<Event> event)
{
    EventTarget* eventTarget = event->target();
    if (eventTarget->toDOMWindow())
        eventTarget->toDOMWindow()->dispatchEvent(event, 0);
    else
        eventTarget->dispatchEvent(event);
}

}
