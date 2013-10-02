/*
 * TaskRegister.h
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#ifndef TASKREGISTER_H_
#define TASKREGISTER_H_

namespace WebCore {

typedef void (*TaskTargetFunction)(void* object, const char* params);

class TaskRegisterMaps;

class TaskRegister {
public:
	TaskRegister();
	virtual ~TaskRegister();

	// Registers that a given object is a target with a given name. When an event is triggered at a target.
	void RegisterTarget(void* object, const char* name, TaskTargetFunction f);

	void UpdateTargetName(void* object, const char* name);

	void UnregisterTarget(void* object);

	// Attempts to run a target with the given name. Returns true on success.
	bool RunTaskOnTarget(const char* target_name, const char* task_params);

private:
	TaskRegisterMaps* m_maps;
};

}  // namespace WebCore

#endif /* TASKREGISTER_H_ */
