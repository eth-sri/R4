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
#include <string>
#include <map>
#include <set>
#include <queue>
#include <vector>

#include <WebCore/platform/EventActionHappensBeforeReport.h>
#include <wtf/ActionLogReport.h>

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

    typedef std::vector<EventActionHandler> ProviderVector;
    typedef std::map<std::string, ProviderVector> TypeToProvider; // the key is the descriptors getType()
    TypeToProvider m_typeToProvider;

    typedef std::queue<EventActionHandler> HandlerQueue;
    typedef std::map<std::string, HandlerQueue> DescriptorToHandler; // the key is the descriptors toString()
    DescriptorToHandler m_descriptorToHandler;

    typedef std::set<std::string> DescriptorSet;
    DescriptorSet m_currentDescriptors; // keys in m_descriptorToHandler
};

EventActionRegister::EventActionRegister()
    : m_maps(new EventActionRegisterMaps)
    , m_isDispatching(false)
    , m_dispatchHistory(new EventActionSchedule())
    , m_verbose(false)
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

void EventActionRegister::registerEventActionHandler(const WTF::EventActionDescriptor& descriptor, EventActionHandlerFunction f, void* object)
{
    std::string key = descriptor.toString();

    EventActionHandler target(f, object);
    m_maps->m_descriptorToHandler[key].push(target);
    m_maps->m_currentDescriptors.insert(key);
}

void EventActionRegister::deregisterEventActionHandler(const WTF::EventActionDescriptor& descriptor)
{
    // TODO(WebERA): This causes an error if there are more than one event action handler with this descriptor, since both will be removed.
    // One solution would be to enforce the "descriptors must be unique" rule.

    ASSERT(!descriptor.isNull());

    std::string key = descriptor.toString();
    m_maps->m_descriptorToHandler.erase(key);
    m_maps->m_currentDescriptors.erase(key);
}

bool EventActionRegister::runEventAction(const WTF::EventActionDescriptor& descriptor) {
        runEventAction(-1, -1, descriptor);
}

bool EventActionRegister::runEventAction(WTF::EventActionId newEventActionId, WTF::EventActionId originalEventActionId, const WTF::EventActionDescriptor& descriptor) {

    std::string descriptorString = descriptor.toString();

    // Match providers
    // Notice, the action log is not used for match providers!

    EventActionRegisterMaps::TypeToProvider::iterator it = m_maps->m_typeToProvider.find(descriptor.getType());

    for (; it != m_maps->m_typeToProvider.end(); it++) {

        const EventActionRegisterMaps::ProviderVector& l = it->second;

        EventActionRegisterMaps::ProviderVector::const_iterator it2 = l.begin();

        for (; it2 != l.end(); it2++) {

            // Note, it is a bit undefined how well the happens before relations are applied if we
            // abort an event action. Thus, HB relations should not be used if event action providers are used.

            WTF::EventActionId eventActionId = newEventActionId == -1 ? HBAllocateEventActionId() : newEventActionId;

            eventActionDispatchStart(eventActionId, originalEventActionId, descriptor);
            HBEnterEventAction(eventActionId, toActionLogType(descriptor.getCategory()));
            ActionLogEventTriggered(l[0].object);

            if (m_verbose) {
                std::cout << "Running " << descriptor.toString() << std::endl; // DEBUG(WebERA)
            }
            bool found = (it2->function)(it2->object, descriptor);

            HBExitEventAction(found);
            eventActionDispatchEnd(found, originalEventActionId);

            if (found) {
                return true; // event handled
            }
        }
    }

    // match handlers, if we find a match then remove the handler

    EventActionRegisterMaps::DescriptorToHandler::iterator iter = m_maps->m_descriptorToHandler.find(descriptorString);

    if (iter == m_maps->m_descriptorToHandler.end()) {
        return false;  // Target with the given name not found.
    }

    EventActionRegisterMaps::HandlerQueue& l = iter->second;
    assert(!l.empty()); // empty HandlerLists should be removed

    // Pre-Execution

    WTF::EventActionId id = newEventActionId == -1 ? HBAllocateEventActionId() : newEventActionId;

    eventActionDispatchStart(id, originalEventActionId, descriptor);
    HBEnterEventAction(id, toActionLogType(descriptor.getCategory()));
    ActionLogEventTriggered(l.front().object);

	// Execute the function.

    if (l.size() > 1) {
        std::cerr << "Warning: multiple targets may fire with signature " << descriptorString << std::endl;
    }

    if (m_verbose) {
        std::cout << "Running " << id << " : " << descriptor.toString() << std::endl; // DEBUG(WebERA)
    }
    bool done = (l.front().function)(l.front().object, descriptor); // don't use the descriptor from this point on, it could be deleted
    ASSERT(done);

    // Cleanup lookup tables

    l.pop();
    if (l.empty()) {
        m_maps->m_descriptorToHandler.erase(descriptorString);
        m_maps->m_currentDescriptors.erase(descriptorString);
    }

    // Post-Execution

    HBExitEventAction(true);
    eventActionDispatchEnd(true, originalEventActionId);

    return true;
}

void EventActionRegister::enterGhostEventAction(WTF::EventActionId id, ActionLog::EventActionType type)
{
    ActionLogStrictMode(false);
    HBEnterEventAction(id, ActionLog::UNKNOWN);
}

void EventActionRegister::exitGhostEventAction()
{
    ActionLogStrictMode(true);
    HBExitEventAction(true);
}

void EventActionRegister::enterImmediateEventAction(ActionLog::EventActionType type, const WTF::EventActionDescriptor& descriptor)
{
    WTF::EventActionId id = HBAllocateEventActionId();

    if (m_verbose) {
        std::cout << "Running[NOW] " << id << " : " << descriptor.toString() << std::endl; // DEBUG(WebERA)
    }

    eventActionDispatchStart(id, -1, descriptor);
    HBEnterEventAction(id, type);
}

void EventActionRegister::exitImmediateEventAction()
{
    HBExitEventAction(true);
    eventActionDispatchEnd(true, -1);
}

void EventActionRegister::debugPrintNames(std::ostream& out) const
{
    out << "Handlers ::" << std::endl;
    EventActionRegisterMaps::DescriptorToHandler::const_iterator it = m_maps->m_descriptorToHandler.begin();
    for (; it != m_maps->m_descriptorToHandler.end(); it++) {
        out << (*it).first << std::endl;
    }

    out << "Providers ::" << std::endl;
    EventActionRegisterMaps::TypeToProvider::const_iterator it2 = m_maps->m_typeToProvider.begin();
    for (; it2 != m_maps->m_typeToProvider.end(); it2++) {
        out << (*it2).first << std::endl;
    }

    out << "--" << std::endl;
}

std::set<std::string> EventActionRegister::getWaitingNames()
{
    return m_maps->m_currentDescriptors;
}

}  // namespace WebCore
