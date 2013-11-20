/*
 * ActionLog.cpp
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#include "ActionLog.h"

const char* ActionLog::CommandType_AsString(CommandType ctype) {
	switch (ctype) {
	case ENTER_SCOPE: return "ENTER_SCOPE";
	case EXIT_SCOPE: return "EXIT_SCOPE";
	case READ_MEMORY: return "READ_MEMORY";
	case WRITE_MEMORY: return "WRITE_MEMORY";
	case TRIGGER_ARC: return "TRIGGER_ARC";
	case MEMORY_VALUE: return "MEMORY_VALUE";
	}
	return "CommandType:OTHER";
}

const char* ActionLog::EventActionType_AsString(EventActionType otype) {
	switch (otype) {
	case UNKNOWN: return "UNKNOWN";
	case TIMER: return "TIMER";
	case USER_INTERFACE: return "USER_INTERFACE";
	case NETWORK: return "NETWORK";
	case CONTINUATION: return "CONTINUATION";
	}
	return "OperationType:OTHER";
}


ActionLog::ActionLog() : m_scopeDepth(0), m_maxEventActionId(-1), m_currentEventActionId(-1) {
}

ActionLog::~ActionLog() {
	for (std::map<int, EventAction*>::iterator it = m_eventActions.begin(); it != m_eventActions.end(); ++it) {
		delete it->second;
	}
}


void ActionLog::addArc(int earlierOperation, int laterOperation, int arcDuration) {
	Arc a;
	a.m_tail = earlierOperation;
	a.m_head = laterOperation;
	a.m_duration = arcDuration;
	m_arcs.push_back(a);
}

void ActionLog::startEventAction(int operation) {
	m_currentEventActionId = operation;
	if (m_eventActions[m_currentEventActionId] == NULL) {
		m_eventActions[m_currentEventActionId] = new EventAction();
	}
	if (operation > m_maxEventActionId) {
		m_maxEventActionId = operation;
	}
	m_cmdsInCurrentEvent.clear();
	m_scopeDepth = 0;
}

bool ActionLog::endEventAction() {
	bool wasInOp = m_currentEventActionId != -1;
	m_currentEventActionId = -1;
	m_cmdsInCurrentEvent.clear();
	m_scopeDepth = 0;
	return wasInOp;
}

bool ActionLog::setEventActionType(EventActionType op_type) {
	if (m_currentEventActionId == -1) return false;
	m_eventActions[m_currentEventActionId]->m_type = op_type;
	return true;
}

bool ActionLog::willLogCommand(CommandType command) {
	if (m_currentEventActionId == -1) return false;
	std::vector<Command>& current_cmds = m_eventActions[m_currentEventActionId]->m_commands;
	if (command == MEMORY_VALUE) {
		if (current_cmds.size() == 0) return false;
		Command& lastc = current_cmds[current_cmds.size() - 1];
		if (lastc.m_cmdType != READ_MEMORY && lastc.m_cmdType != WRITE_MEMORY) {
			return false;
		}
	}
	return true;
}

bool ActionLog::logCommand(CommandType command, int memoryLocation) {
	if (m_currentEventActionId == -1) return false;
	if (!willLogCommand(command)) return true;
	Command c;
	c.m_cmdType = command;
	c.m_location = memoryLocation;
	if (command == READ_MEMORY || command == WRITE_MEMORY) {
		if (!m_cmdsInCurrentEvent.insert(c).second) {
			return true;  // Already exists, no need to add again to the same op.
		}
	}
	std::vector<Command>& current_cmds = m_eventActions[m_currentEventActionId]->m_commands;
	if (command == ENTER_SCOPE) ++m_scopeDepth;
	if (command == EXIT_SCOPE) --m_scopeDepth;
	if (command == EXIT_SCOPE &&
		current_cmds.size() > 0 &&
		current_cmds[current_cmds.size() - 1].m_cmdType == ENTER_SCOPE) {
		current_cmds.pop_back();  // Remove the last enter scope. There was nothing in it and we exit it.
		return true;
	}
	current_cmds.push_back(c);
	return true;
}

void ActionLog::triggerEvent(void* eventId) {
	if (m_currentEventActionId == -1) return;
	PendingTriggerArc& pending_arc = m_pendingTriggerArcs[reinterpret_cast<long int>(eventId)];

	std::vector<Command>& current_cmds = m_eventActions[m_currentEventActionId]->m_commands;
	pending_arc.m_operationId = m_currentEventActionId;
	pending_arc.m_commandId = current_cmds.size();

	Command c;
	c.m_cmdType = ActionLog::TRIGGER_ARC;
	c.m_location = -1;
	current_cmds.push_back(c);
}

void ActionLog::eventTriggered(void* eventId) {
	if (m_currentEventActionId == -1) return;
	PendingTriggerArcs::iterator it = m_pendingTriggerArcs.find(reinterpret_cast<long int>(eventId));
	if (it == m_pendingTriggerArcs.end()) return;
	m_eventActions[it->second.m_operationId]->m_commands[it->second.m_commandId].m_location = m_currentEventActionId;
	m_pendingTriggerArcs.erase(it);
}

struct ActionLogHeader {
	int num_ops;
	int num_arcs;
};

struct OperationHeader {
	int id;
	ActionLog::EventActionType type;
	int num_commands;
};

void ActionLog::saveToFile(FILE* f) {
	ActionLogHeader hdr;
	hdr.num_arcs = m_arcs.size();
	hdr.num_ops = m_eventActions.size();
	fwrite(&hdr, sizeof(hdr), 1, f);
	fwrite(m_arcs.data(), sizeof(Arc), m_arcs.size(), f);
	for (EventActionSet::const_iterator it = m_eventActions.begin(); it != m_eventActions.end(); ++it) {
		OperationHeader ophdr;
		const EventAction& op = *(it->second);
		ophdr.id = it->first;
		ophdr.type = op.m_type;
		ophdr.num_commands = op.m_commands.size();
		fwrite(&ophdr, sizeof(ophdr), 1, f);
		fwrite(op.m_commands.data(), sizeof(Command), op.m_commands.size(), f);
	}
	fflush(f);
	printf("Action log saved.\n");
}

bool ActionLog::loadFromFile(FILE* f) {
	ActionLogHeader hdr;
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) return false;
	m_arcs.resize(hdr.num_arcs);
	if (fread(m_arcs.data(), sizeof(Arc), m_arcs.size(), f) != m_arcs.size()) return false;
	for (int i = 0; i < hdr.num_ops; ++i) {
		OperationHeader ophdr;
		if (fread(&ophdr, sizeof(ophdr), 1, f) != 1) return false;
		EventAction* op = new EventAction();
		op->m_commands.resize(ophdr.num_commands);
		op->m_type = ophdr.type;
		if (fread(op->m_commands.data(), sizeof(Command), op->m_commands.size(), f) !=
				op->m_commands.size()) {
			return false;
		}
		m_eventActions[ophdr.id] = op;
		if (ophdr.id > m_maxEventActionId) {
			m_maxEventActionId = ophdr.id;
		}
	}
	return true;
}

