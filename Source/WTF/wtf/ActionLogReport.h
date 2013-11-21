/*
 * ActionLogReport.h
 *
 *  Created on: May 11, 2012
 *      Author: veselin
 */

#ifndef ACTIONLOGREPORT_H_
#define ACTIONLOGREPORT_H_

#include "ActionLog.h"

class ActionLogScope {
public:
	ActionLogScope(const char* name);
	~ActionLogScope();
};

void ActionLogStrictMode(bool strict);
bool ActionLogInStrictMode();

void ActionLogScopeStart(const char* name);
void ActionLogScopeEnd();

void ActionLogReportMemoryValue(const char* value);

void ActionLogReportArrayRead(size_t array, int index);  // Reads from a single array index.
void ActionLogReportArrayWrite(size_t array, int index);  // Write to a single array index.
void ActionLogReportArrayReadScan(size_t array, int index);  // Reads a cell by scanning it.
void ActionLogReportArrayReadLen(size_t array);  // Reads the array length.
void ActionLogReportArrayModify(size_t array);  // Writes to more than one array element or resizes an array.

void ActionLogFormat(ActionLog::CommandType cmd, const char* format, ...);
bool ActionLogWillAddCommand(ActionLog::CommandType cmd);

void ActionLogEnterOperation(int id, ActionLog::EventActionType type);
void ActionLogExitOperation();

int ActionLogScopeDepth();

void ActionLogAddArc(int earlierId, int laterId, int duration);
void ActionLogSave();

// Logs that an event identified by a pointer eventId is triggered node.
void ActionLogTriggerEvent(void* eventId);
// Logs that the id of the currently entered operation is the one triggered by a
// previous call of ActionLogTriggerEvent with the same eventId.
void ActionLogEventTriggered(void* eventId);

int ActionLogRegisterSource(const char* src);

class EventAttachLog {
public:
	EventAttachLog();
	virtual ~EventAttachLog();

	enum EventType {
		EV_KEYDOWN = 0,
		EV_KEYUP,
		EV_KEYPRESS,
		EV_CHANGE,
		EV_RESIZE,
		EV_FOCUS,
		EV_BLUR,
		EV_MOUSEOVER,
		EV_MOUSEOUT,
		EV_MOUSEUP,
		EV_MOUSEDOWN,
		EV_MOUSEMOVE,
		EV_DBLCLICK,
		EV_CLICK,
		EV_NUM_EVENTS
	};

	static const char* EventTypeStr(EventType t);
    static EventAttachLog::EventType StrEventType(const char* t);

	void addEventStr(void* eventTarget, const char* str);

	virtual void removeEventTarget(void* eventTarget) = 0;
	virtual void addEvent(void* eventTarget, EventType eventType) = 0;
	virtual bool pullEvent(void** eventTarget, EventType* eventType) = 0;
};

EventAttachLog* getEventAttachLog();

#endif /* ACTIONLOGREPORT_H_ */
