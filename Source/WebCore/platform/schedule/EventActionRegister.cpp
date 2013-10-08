/*
 * EventActionRegister.cpp
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#include "EventActionRegister.h"

#include <iostream>
#include <stdio.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace WebCore {

struct EventActionProvider {
	void* object;
    EventActionProviderFunction function;
};


class EventActionRegisterMaps {
public:
    typedef std::vector<EventActionProvider> ProviderList;
    typedef std::map<std::string, ProviderList> NameToProvider;
    NameToProvider m_nameToProvider;

    typedef std::map<void*, std::string> ProviderToName;
    ProviderToName m_providerToName;
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

void EventActionRegister::registerEventActionProvider(void* object, const char* name, EventActionProviderFunction f) {
    unregisterActionProvider(object);

    EventActionProvider target;
	target.object = object;
	target.function = f;
    m_maps->m_nameToProvider[name].push_back(target);
    m_maps->m_providerToName[object] = name;
}

void EventActionRegister::updateActionProviderName(void* object, const char* name) {
    EventActionRegisterMaps::ProviderToName::iterator it =
            m_maps->m_providerToName.find(object);
    if (it == m_maps->m_providerToName.end() ||  // Name not found.
			it->second == name) return;  // Or nothing to update.

    EventActionRegisterMaps::NameToProvider::iterator it2 =
            m_maps->m_nameToProvider.find(it->second);  // Map from name to TargetList
    EventActionRegisterMaps::ProviderList& l = it2->second;

	// Get the function registered for this target.
    EventActionProviderFunction f = NULL;
	for (int i = l.size(); i > 0;) {
		--i;
		if (l[i].object == object) {
			f = l[i].function;
			l.erase(l.begin() + i);
		}
	}

	// Reregister under the new name.
	if (f != NULL) {
        registerEventActionProvider(object, name, f);
	}
}

void EventActionRegister::unregisterActionProvider(void* object) {
    EventActionRegisterMaps::ProviderToName::iterator it =
            m_maps->m_providerToName.find(object);
    if (it == m_maps->m_providerToName.end()) return;  // Nothing to remove.
    EventActionRegisterMaps::NameToProvider::iterator it2 =
            m_maps->m_nameToProvider.find(it->second);  // Map from name to TargetList
    EventActionRegisterMaps::ProviderList& l = it2->second;
	for (int i = l.size(); i > 0;) {
		--i;
		if (l[i].object == object) l.erase(l.begin() + i);
	}
    if (l.empty()) m_maps->m_nameToProvider.erase(it2);
    m_maps->m_providerToName.erase(it);
}

bool EventActionRegister::runEventAction(const EventActionDescriptor& descriptor) {
    std::string name = descriptor.getName();
    std::string params = descriptor.getParams();

    EventActionRegisterMaps::NameToProvider::iterator it =
            m_maps->m_nameToProvider.find(name);

    if (it == m_maps->m_nameToProvider.end()) {
        return false;  // Target with the given name not found.
    }

    const EventActionRegisterMaps::ProviderList& l = it->second;

    if (l.empty()) {
        return false; // No provider for target
    }

    eventActionDispatchStart(descriptor);

	// Execute the function.
    if (l.size() > 1) {
        std::cerr << "Warning: multiple targets may fire with name " << name << ", params " << params << std::endl;
    }

    std::cout << "Running " << name << "(" << params << ")" << std::endl; // TODO(WebERA): DEBUG
    bool result = (l[0].function)(l[0].object, params);

    eventActionDispatchEnd();

    return result;
}

EventActionDescriptor EventActionRegister::allocateEventDescriptor(const std::string& name, const std::string& params)
{
    return EventActionDescriptor(m_nextEventActionDescriptorId++, name, params);
}

void EventActionRegister::debugPrintNames() const
{
    EventActionRegisterMaps::NameToProvider::const_iterator it = m_maps->m_nameToProvider.begin();
    for (; it != m_maps->m_nameToProvider.end(); it++) {
        std::cout << (*it).first << std::endl;
    }
}

}  // namespace WebCore
