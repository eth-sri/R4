/*
 * ActionLog.h
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#ifndef ACTIONLOG_H_
#define ACTIONLOG_H_

#include <stdio.h>
#include <map>
#include <set>
#include <vector>

class ActionLog {
public:
	ActionLog();
	~ActionLog();

	enum CommandType {
		ENTER_SCOPE = 0,
		EXIT_SCOPE,
		READ_MEMORY,
		WRITE_MEMORY,
		TRIGGER_ARC,
		MEMORY_VALUE
	};
	static const char* CommandType_AsString(CommandType ctype);

	enum EventActionType {
		UNKNOWN = 0,
		TIMER,
		USER_INTERFACE,
		NETWORK,
		CONTINUATION
	};
	static const char* EventActionType_AsString(EventActionType otype);

	// Adds an arcs. Doesn't check for duplicates or validity.
	void addArc(int earlierOperation, int laterOperation, int arcDuration);

	// Starts an event action. Only one event action can be started at a time.
	void startEventAction(int event_action_id);

	// Exits the currently started event action. Returns false if not in an event action.
	bool endEventAction();

	// Sets the type of the current event action. Returns false if not in an event action.
	bool setEventActionType(EventActionType op_type);

	// Enters a scope in the current operation. Returns false if not in an event action.
	bool enterScope(int scopeId) {
		return logCommand(ENTER_SCOPE, scopeId);
	}
	// Exits the last entered scope.
	bool exitScope() {
		return logCommand(EXIT_SCOPE, -1);
	}

	// The number of open scopes.
	int scopeDepth() const {
		return m_scopeDepth;
	}

	// Returns whether logs of a certain command type will be written to the log.
	// For example, MEMORY_VALUE can only be written after a read or a write.
	bool willLogCommand(CommandType command);

	// Logs a command. Returns false if not in an operation.
	bool logCommand(CommandType command, int memoryLocation);

	// Logs that an event identified by a pointer eventId is triggered node.
	void triggerEvent(void* eventId);

	// Logs that the id of the currently entered operation is the one triggered by a
	// previous call of triggerEvent with the same eventId.
	void eventTriggered(void* eventId);

	// Saves the log to a file.
	void saveToFile(FILE* f);

	// Loads from log from a file.
	bool loadFromFile(FILE* f);

	struct Command {
		CommandType m_cmdType;
		// Memory location for reads/writes and scope id for scopes. Should be -1 if the location is unused.
		int m_location;

		bool operator==(const Command& o) const { return m_cmdType == o.m_cmdType && m_location == o.m_location; }
		bool operator<(const Command& o) const {
			return (m_cmdType == o.m_cmdType) ? (m_location < o.m_location) : (m_cmdType < o.m_cmdType);
		}
	};

	struct Arc {
		int m_tail;
		int m_head;
		// The duration of the arc, -1 if the duration is unknown.
		int m_duration;
	};

	struct EventAction {
		EventAction() : m_type(UNKNOWN) {}

		EventActionType m_type;
		std::vector<Command> m_commands;
	};

	const std::vector<Arc>& arcs() const { return m_arcs; }
	const EventAction& event_action(int i) const {
		EventActionSet::const_iterator it = m_eventActions.find(i);
		if (it == m_eventActions.end()) {
			return m_emptyEventAction;
		}
		return *(it->second);
	}
	int maxEventActionId() const { return m_maxEventActionId; }

private:
	struct PendingTriggerArc {
		int m_operationId;
		int m_commandId;
	};
	typedef std::map<long int, PendingTriggerArc> PendingTriggerArcs;

	typedef std::map<int, EventAction*> EventActionSet;
	EventAction m_emptyEventAction;
	EventActionSet m_eventActions;
	int m_scopeDepth;
	int m_maxEventActionId;
	std::vector<Arc> m_arcs;
	PendingTriggerArcs m_pendingTriggerArcs;

	// Fields to help construction.
	int m_currentEventActionId;
	std::set<Command> m_cmdsInCurrentEvent;
};

#endif /* ACTIONLOG_H_ */
