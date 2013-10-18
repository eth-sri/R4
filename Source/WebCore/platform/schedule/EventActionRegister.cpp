/*
 * EventActionRegister.cpp
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#include "EventActionRegister.h"

#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace WebCore {

struct EventActionHandler {
    void* object; // some event handler specific payload
    EventActionHandlerFunction function;

    EventActionHandler(EventActionHandlerFunction function, void* object)
        : object(object)
        , function(function)
    {}
};


class EventActionRegisterMaps {
public:
    typedef std::vector<EventActionHandler> HandlerList;

    typedef std::map<std::string, HandlerList> TypeToProvider; // the key is the descriptors getType()
    TypeToProvider m_typeToProvider;

    typedef std::map<std::string, HandlerList> DescriptorToHandler; // the key is the descriptors toString()
    TypeToProvider m_descriptorToHandler;
};

EventActionRegister::EventActionRegister()
    : m_maps(new EventActionRegisterMaps)
    , m_isDispatching(false)
    , m_nextEventActionDescriptorId(0)
    , m_dispatchHistory(new EventActionSchedule())
{
}

EventActionRegister::~EventActionRegister() {
	delete m_maps;
    delete m_dispatchHistory;
}

void EventActionRegister::registerEventActionProvider(const std::string& type, EventActionHandlerFunction f, void* object)
{
    EventActionHandler target(f, object);
    m_maps->m_typeToProvider[type].push_back(target);
}

void EventActionRegister::registerEventActionHandler(const EventActionDescriptor& descriptor, EventActionHandlerFunction f, void* object)
{
    EventActionHandler target(f, object);
    m_maps->m_descriptorToHandler[descriptor.toString()].push_back(target);
}

bool EventActionRegister::runEventAction(const EventActionDescriptor& descriptor) {

    std::string descriptorString = descriptor.toString();

    // match providers

    EventActionRegisterMaps::TypeToProvider::iterator it = m_maps->m_typeToProvider.find(descriptor.getType());

    for (; it != m_maps->m_typeToProvider.end(); it++) {

        const EventActionRegisterMaps::HandlerList& l = it->second;

        EventActionRegisterMaps::HandlerList::const_iterator it2 = l.begin();

        for (; it2 != l.end(); it2++) {

            eventActionDispatchStart(descriptor);
            std::cout << "Running " << descriptor.toString() << std::endl; // TODO(WebERA): DEBUG
            bool result = (it2->function)(it2->object, descriptor);
            eventActionDispatchEnd(result);

            if (result) {
                return true; // event handled
            }
        }
    }

    // match handlers, if we find a match then remove the handler

    EventActionRegisterMaps::DescriptorToHandler::iterator iter = m_maps->m_descriptorToHandler.find(descriptorString);

    if (iter == m_maps->m_descriptorToHandler.end()) {
        return false;  // Target with the given name not found.
    }

    EventActionRegisterMaps::HandlerList& l = iter->second;
    assert(!l.empty()); // empty HandlerLists should be removed

    eventActionDispatchStart(descriptor);

	// Execute the function.
    if (l.size() > 1) {
        std::cerr << "Warning: multiple targets may fire with signature " << descriptorString << std::endl;
    }

    std::cout << "Running " << descriptor.toString() << std::endl; // TODO(WebERA): DEBUG
    bool result = (l[0].function)(l[0].object, descriptor); // don't use the descriptor from this point on, it could be deleted

    if (result) {
        l.erase(l.begin());
        if (l.empty()) {
            m_maps->m_descriptorToHandler.erase(descriptorString);
        }
    }

    eventActionDispatchEnd(result);

    return result;
}

EventActionDescriptor EventActionRegister::allocateEventDescriptor(const std::string& type, const std::string& params)
{
    return EventActionDescriptor(m_nextEventActionDescriptorId++, type, params);
}

void EventActionRegister::debugPrintNames() const
{
    EventActionRegisterMaps::DescriptorToHandler::const_iterator it = m_maps->m_descriptorToHandler.begin();
    for (; it != m_maps->m_descriptorToHandler.end(); it++) {
        std::cout << (*it).first << std::endl;
    }
}

std::vector<std::string> EventActionRegister::getWaitingNames()
{
    // TODO(WebERA): This has very poor performance, we should maintain a list of keys

    std::vector<std::string> v;
    for(EventActionRegisterMaps::DescriptorToHandler::iterator it = m_maps->m_descriptorToHandler.begin(); it != m_maps->m_descriptorToHandler.end(); ++it) {
      v.push_back(it->first);
    }

    return v;
}

}  // namespace WebCore
