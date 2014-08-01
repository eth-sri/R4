/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventSender_h
#define EventSender_h

#include <wtf/ExportMacros.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>

#include <sstream>
#include <utility>
#include <map>

#include "WebCore/platform/Timer.h"
#include "WebCore/platform/ThreadTimers.h"
#include <WebCore/platform/EventActionHappensBeforeReport.h>

#include <wtf/Vector.h>
#include <wtf/EventActionDescriptor.h>
#include <wtf/ActionLogReport.h>

namespace WebCore {

template<typename T> class EventSenderEvent {
    WTF_MAKE_NONCOPYABLE(EventSenderEvent); WTF_MAKE_FAST_ALLOCATED;

public:
    explicit EventSenderEvent(void* parent, T* sender, const AtomicString& eventType, const std::string& owner, const std::string& identifier);
    ~EventSenderEvent() { cancelEvent(); }

    void dispatchEvent();
    void cancelEvent();

    bool isPending() { return !m_dispatched; }

private:
    void timerFired(Timer<EventSenderEvent<T> >*) { dispatchEvent(); }

    void* m_parent;
    T* m_sender;
    Timer<EventSenderEvent<T> > m_timer;
    bool m_dispatched;
    WTF::EventActionId m_parentEventAction;
};

template<typename T> class EventSender {
    WTF_MAKE_NONCOPYABLE(EventSender); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit EventSender(const AtomicString& eventType);
    ~EventSender();

    const AtomicString& eventType() const { return m_eventType; }
    void dispatchEventSoon(T*, const std::string&, const std::string&);
    void cancelEvent(T*);
    void dispatchPendingEvents();

#ifndef NDEBUG
    bool hasPendingEvents(T* sender) const;
#endif

private:
    AtomicString m_eventType;

    std::multimap<T*, EventSenderEvent<T>*> m_dispatchMap;
};

// EventSenderEvent

template<typename T> EventSenderEvent<T>::EventSenderEvent(void* parent, T* sender, const AtomicString& eventType, const std::string& owner, const std::string& identifier)
    : m_parent(parent)
    , m_sender(sender)
    , m_timer(this, &EventSenderEvent::timerFired)
    , m_dispatched(false)
    , m_parentEventAction(HBCurrentEventAction())
{
    std::stringstream params;
    params << eventType.string().ascii().data() << ",";
    params << owner << ",";
    params << identifier;

    WTF::EventActionDescriptor descriptor(WTF::OTHER, "EventSender", params.str());

    m_timer.setEventActionDescriptor(descriptor);
    m_timer.startOneShot(0);
}

template<typename T> void EventSenderEvent<T>::dispatchEvent()
{
    if (m_dispatched) {
        return;
    }

    m_dispatched = true;

    ActionLogScope s("dispatch-event");
    m_sender->dispatchPendingEvent(static_cast<EventSender<T>*>(m_parent));

    HBAddExplicitArc(m_parentEventAction, HBCurrentEventAction());
}

template<typename T> void EventSenderEvent<T>::cancelEvent()
{
    m_dispatched = true;
}

// EventSender

template<typename T> EventSender<T>::EventSender(const AtomicString& eventType)
    : m_eventType(eventType)
{
}

template<typename T> EventSender<T>::~EventSender()
{
    for (typename std::multimap<T*, EventSenderEvent<T>* >::iterator it = m_dispatchMap.begin(); it != m_dispatchMap.end(); it++) {
        delete (*it);
    }
}

template<typename T> void EventSender<T>::dispatchEventSoon(T* sender, const std::string& owner, const std::string& identifier)
{
    m_dispatchMap.insert(std::pair<T*, EventSenderEvent<T>*>(sender, new EventSenderEvent<T>(this, sender, m_eventType, owner, identifier)));
}

template<typename T> void EventSender<T>::cancelEvent(T* sender)
{
    std::pair<typename std::multimap<T*, EventSenderEvent<T>*>::iterator, typename std::multimap<T*, EventSenderEvent<T>*>::iterator> result = m_dispatchMap.equal_range(sender);

    for (typename std::multimap<T*, EventSenderEvent<T>*>::iterator it = result.first; it != result.second; it++) {
        (*it).second->cancelEvent();
    }
}

template<typename T> void EventSender<T>::dispatchPendingEvents()
{
    for (typename std::multimap<T*, EventSenderEvent<T>*>::iterator it = m_dispatchMap.begin(); it != m_dispatchMap.end(); it++) {
        (*it).second->dispatchEvent(); // WebERA: TODO, decide if this should be allowed. In some cases this results in some very restrictive HB relations
    }
}

#ifndef NDEBUG
template<typename T> bool EventSender<T>::hasPendingEvents(T* sender) const
{
    std::pair<typename std::multimap<T*, EventSenderEvent<T>*>::const_iterator, typename std::multimap<T*, EventSenderEvent<T>*>::const_iterator> result = m_dispatchMap.equal_range(sender);

    for (typename std::multimap<T*, EventSenderEvent<T>*>::const_iterator it = result.first; it != result.second; it++) {
        if ((*it).second->isPending()) {
            return true;
        }
    }

    return false;
}
#endif

} // namespace WebCore

#endif // EventSender_h
