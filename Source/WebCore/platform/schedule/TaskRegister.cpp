/*
 * TaskRegister.cpp
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#include "TaskRegister.h"

#include <stdio.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace WebCore {

struct TaskRegisterTarget {
	void* object;
	TaskTargetFunction function;
};


class TaskRegisterMaps {
public:
	typedef std::vector<TaskRegisterTarget> TargetList;
	typedef std::map<std::string, TargetList> NameToTarget;
	NameToTarget m_nameToTarget;

	typedef std::map<void*, std::string> TargetToName;
	TargetToName m_targetToName;
};

TaskRegister::TaskRegister() : m_maps(new TaskRegisterMaps) {
}

TaskRegister::~TaskRegister() {
	delete m_maps;
}

void TaskRegister::RegisterTarget(void* object, const char* name, TaskTargetFunction f) {
	UnregisterTarget(object);

	TaskRegisterTarget target;
	target.object = object;
	target.function = f;
	m_maps->m_nameToTarget[name].push_back(target);
	m_maps->m_targetToName[object] = name;
}

void TaskRegister::UpdateTargetName(void* object, const char* name) {
	TaskRegisterMaps::TargetToName::iterator it =
			m_maps->m_targetToName.find(object);
	if (it == m_maps->m_targetToName.end() ||  // Name not found.
			it->second == name) return;  // Or nothing to update.

	TaskRegisterMaps::NameToTarget::iterator it2 =
			m_maps->m_nameToTarget.find(it->second);  // Map from name to TargetList
	TaskRegisterMaps::TargetList& l = it2->second;

	// Get the function registered for this target.
	TaskTargetFunction f = NULL;
	for (int i = l.size(); i > 0;) {
		--i;
		if (l[i].object == object) {
			f = l[i].function;
			l.erase(l.begin() + i);
		}
	}

	// Reregister under the new name.
	if (f != NULL) {
		RegisterTarget(object, name, f);
	}
}

void TaskRegister::UnregisterTarget(void* object) {
	TaskRegisterMaps::TargetToName::iterator it =
			m_maps->m_targetToName.find(object);
	if (it == m_maps->m_targetToName.end()) return;  // Nothing to remove.
	TaskRegisterMaps::NameToTarget::iterator it2 =
			m_maps->m_nameToTarget.find(it->second);  // Map from name to TargetList
	TaskRegisterMaps::TargetList& l = it2->second;
	for (int i = l.size(); i > 0;) {
		--i;
		if (l[i].object == object) l.erase(l.begin() + i);
	}
	if (l.empty()) m_maps->m_nameToTarget.erase(it2);
	m_maps->m_targetToName.erase(it);
}

bool TaskRegister::RunTaskOnTarget(const char* name, const char* params) {
	TaskRegisterMaps::NameToTarget::iterator it2 =
			m_maps->m_nameToTarget.find(name);
	if (it2 == m_maps->m_nameToTarget.end()) return false;  // Target with the given name not found.
	const TaskRegisterMaps::TargetList& l = it2->second;
	if (l.empty()) return false;

	// Execute the function.
	if (l.size() > 1)
		fprintf(stderr, "Warning: multiple targets may fire with name %s, params %s\n", name, params);
	printf("Running %s %s\n", name, params);
	return (l[0].function)(l[0].object, params);
}

}  // namespace WebCore
