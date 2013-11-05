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

#endif /* ACTIONLOGREPORT_H_ */
