/*
 * ActionLogReport.cpp
 *
 *  Created on: May 11, 2012
 *      Author: veselin
 */

#include "config.h"

#include <stdio.h>
#include "Assertions.h"
#include "ActionLogReport.h"
#include "WTFThreadData.h"
#include "StringSet.h"

#include <set>
#include <queue>

ActionLogScope::ActionLogScope(const char* name) {
	ActionLogScopeStart(name);
}

ActionLogScope::~ActionLogScope() {
	ActionLogScopeEnd();
}

static bool strict_mode = true;

// Disable some consistency checks when replaying and after saving the log (just before a recording is closed). Some stray events can otherwise cause problems.
void ActionLogStrictMode(bool strict) {
    strict_mode = strict;
}

// TODO(WebERA): Remove this, mostly this requires us to find a more proper way of saving a log and shutting down WebKit
bool ActionLogInStrictMode() {
    return strict_mode;
}

void ActionLogScopeStart(const char* name) {
//	printf("Scope %s\n", name);
	int scopeId = wtfThreadData().scopeSet()->addString(name);
	if (!wtfThreadData().actionLog()->enterScope(scopeId) && strict_mode) {
		fprintf(stderr, "Can't log start scope %s\n", name);
        CRASH();
	}
}

void ActionLogScopeEnd() {
//	printf("Endscope\n");
    if (!wtfThreadData().actionLog()->exitScope() && strict_mode) {
        fprintf(stderr, "Can't log end scope\n");
        CRASH();
    }
}

void ActionLogFormatV(ActionLog::CommandType cmd, const char* format, va_list ap) {
	char strspace[512] = { 0 };
	// It's possible for methods that use a va_list to invalidate
	// the data in it upon use.  The fix is to make a copy
	// of the structure before using it and use that copy instead.
	va_list backup_ap;
	va_copy(backup_ap, ap);
	vsnprintf(strspace, sizeof(strspace) - 1, format, ap);
	va_end(backup_ap);

	// Use the string even if it didn't fit in the 256 chars.

    int stringId;
    if (cmd == ActionLog::MEMORY_VALUE) {
        stringId = wtfThreadData().dataSet()->addString(strspace);
    } else if (cmd == ActionLog::ENTER_SCOPE) {
        stringId = wtfThreadData().scopeSet()->addString(strspace);
    } else {
        stringId = wtfThreadData().variableSet()->addString(strspace);
    }
    if (!wtfThreadData().actionLog()->logCommand(cmd, stringId) && strict_mode) {
        fprintf(stderr, "Can't log command %s %s\n", ActionLog::CommandType_AsString(cmd), strspace);
        CRASH();
    }

//	printf("%s %s\n", ActionLog::CommandType_AsString(cmd), strspace);
}

void ActionLogFormat(ActionLog::CommandType cmd, const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	ActionLogFormatV(cmd, format, ap);
	va_end(ap);
}

void ActionLogReportArrayRead(size_t array, int index) {
	ActionLogFormat(ActionLog::READ_MEMORY, "Array[%d]$LEN", static_cast<int>(array));
	ActionLogFormat(ActionLog::READ_MEMORY, "Array[%d]$[%d]", static_cast<int>(array), index);
}

void ActionLogReportArrayWrite(size_t array, int index) {
	ActionLogFormat(ActionLog::READ_MEMORY, "Array[%d]$LEN", static_cast<int>(array));
	ActionLogFormat(ActionLog::WRITE_MEMORY, "Array[%d]$[%d]", static_cast<int>(array), index);
}

void ActionLogReportArrayReadScan(size_t array, int index) {
	ActionLogFormat(ActionLog::READ_MEMORY, "Array[%d]$[%d]", static_cast<int>(array), index);
}

void ActionLogReportArrayReadLen(size_t array) {
	ActionLogFormat(ActionLog::READ_MEMORY, "Array[%d]$LEN", static_cast<int>(array));
	// Read through all cells.
	//ActionLogFormat(ActionLog::READ_MEMORY, "Array[%p]$W", static_cast<int>(array));
}

void ActionLogReportArrayModify(size_t array) {
	ActionLogFormat(ActionLog::WRITE_MEMORY, "Array[%d]$LEN", static_cast<int>(array));
}

void ActionLogReportMemoryValue(const char* value) {
    int stringId = wtfThreadData().dataSet()->addString(value);
    if (!wtfThreadData().actionLog()->logCommand(ActionLog::MEMORY_VALUE, stringId) && strict_mode) {
        fprintf(stderr, "Can't log value %s\n", value);
        CRASH();
    }
}

void ActionLogEnterOperation(int id, ActionLog::EventActionType type) {
    wtfThreadData().actionLog()->startEventAction(id);
    if (!wtfThreadData().actionLog()->setEventActionType(type) && strict_mode) {
        fprintf(stderr, "Can't set optype %s\n", ActionLog::EventActionType_AsString(type));
        CRASH();
    }
}

void ActionLogExitOperation() {
    if (!wtfThreadData().actionLog()->endEventAction() && strict_mode) {
        fprintf(stderr, "Can't log exit op.\n");
        CRASH();
    }
}

int ActionLogScopeDepth() {
	return wtfThreadData().actionLog()->scopeDepth();
}

void ActionLogAddArc(int earlierId, int laterId, int duration) {
	if (laterId <= earlierId) {
		fprintf(stderr, "Invalid arc %d -> %d\n", earlierId, laterId);
		ActionLogSave();
		CRASH();
	}
	wtfThreadData().actionLog()->addArc(earlierId, laterId, duration);
}

void ActionLogAddArcEvent(int nextId) {
    if (!wtfThreadData().actionLog()->logCommand(ActionLog::TRIGGER_ARC, nextId) && strict_mode) {
        fprintf(stderr, "Can't log add arc to %d\n", nextId);
        CRASH();
    }
}

void ActionLogTriggerEvent(void* eventId) {
	wtfThreadData().actionLog()->triggerEvent(eventId);
}

void ActionLogEventTriggered(void* eventId) {
	wtfThreadData().actionLog()->eventTriggered(eventId);
}

int ActionLogRegisterSource(const char* src) {
	return wtfThreadData().jsSet()->addString(src);
}

bool ActionLogWillAddCommand(ActionLog::CommandType cmd) {
	return wtfThreadData().actionLog()->willLogCommand(cmd);
}

void ActionLogSave() {
	FILE* f = fopen("/tmp/ER_actionlog", "wb");
	wtfThreadData().variableSet()->saveToFile(f);
	wtfThreadData().scopeSet()->saveToFile(f);
	wtfThreadData().actionLog()->saveToFile(f);
	wtfThreadData().jsSet()->saveToFile(f);
	wtfThreadData().dataSet()->saveToFile(f);
	fclose(f);
}


EventAttachLog::EventAttachLog() {
}
EventAttachLog::~EventAttachLog() {
}
const char* EventAttachLog::EventTypeStr(EventType t) {
	switch (t) {
	case EV_KEYDOWN: return "keydown";
	case EV_KEYUP: return "keyup";
	case EV_KEYPRESS: return "keypress";
	case EV_RESIZE: return "resize";
	case EV_MOUSEMOVE: return "mousemove";
	case EV_MOUSEOUT: return "mouseout";
	case EV_MOUSEOVER: return "mouseover";
	case EV_MOUSEDOWN: return "mousedown";
	case EV_MOUSEUP: return "mouseup";
	case EV_CHANGE: return "change";
	case EV_CLICK: return "click";
	case EV_DBLCLICK: return "dblclick";
	case EV_FOCUS: return "focus";
	case EV_BLUR: return "blur";
	}
	return "";
}

EventAttachLog::EventType EventAttachLog::StrEventType(const char* str) {

    if (strcmp(str, "keydown") == 0) {
        return EV_KEYDOWN;
    } else if (strcmp(str, "keyup") == 0) {
        return EV_KEYUP;
    } else if (strcmp(str, "keypress") == 0) {
        return EV_KEYPRESS;
    } else if (strcmp(str, "resize") == 0) {
        return EV_RESIZE;
    } else if (strcmp(str, "mousemove") == 0) {
        return EV_MOUSEMOVE;
    } else if (strcmp(str, "mouseout") == 0) {
        return EV_MOUSEOUT;
    } else if (strcmp(str, "mouseover") == 0) {
        return EV_MOUSEOVER;
    } else if (strcmp(str, "mousedown") == 0) {
        return EV_MOUSEDOWN;
    } else if (strcmp(str, "mouseup") == 0) {
        return EV_MOUSEUP;
    } else if (strcmp(str, "click") == 0) {
        return EV_CLICK;
    } else if (strcmp(str, "dblclick") == 0) {
        return EV_DBLCLICK;
    } else if (strcmp(str, "focus") == 0) {
        return EV_FOCUS;
    } else if (strcmp(str, "blur") == 0) {
        return EV_BLUR;
    } else if (strcmp(str, "change") == 0) {
        return EV_CHANGE;
    }

    return EV_NUM_EVENTS; // unknown/unsupported event
}

void EventAttachLog::addEventStr(void* eventTarget, const char* str) {

    EventAttachLog::EventType type = EventAttachLog::StrEventType(str);

    if (type == EV_CLICK) {
        // Note(veselin): Currently we will not auto-trigger event when a click handler is attached.
        return;
    }

    if (type == EV_NUM_EVENTS) {
        // Unknown event
        return;
    }

    addEvent(eventTarget, type);
}

class EventAttachLogImpl : public EventAttachLog {
public:
	EventAttachLogImpl() {}
	virtual ~EventAttachLogImpl() {}

    virtual void removeEventTarget(void* eventTarget) {
		for (int i = 0; i < EV_NUM_EVENTS; ++i) {
			int l = m_queues[i].size();
			for (int j = 0; j < l; ++j) {
				if (m_queues[i].empty()) break;
				if (m_queues[i].front() != eventTarget) {
					m_queues[i].push_back(m_queues[i].front());
				}
				m_queues[i].pop_front();
			}
		}
	}

    virtual void addEvent(void* eventTarget, EventType eventType) {
		m_queues[eventType].push_back(eventTarget);
	}

    virtual bool pullEvent(void** eventTarget, EventType* eventType) {
		for (int i = 0; i < EV_NUM_EVENTS; ++i) {
			if (!m_queues[i].empty()) {
                *eventType = static_cast<EventType>(i);
				*eventTarget = m_queues[i].front();
                m_queues[i].pop_front();
				return true;
			}
		}
		return false;
	}

private:
    std::deque<void*> m_queues[EV_NUM_EVENTS];
};

EventAttachLog* getEventAttachLog() {
	EventAttachLog* result = wtfThreadData().eventAttachLog();
	if (result == NULL) {
		result = new EventAttachLogImpl();
		wtfThreadData().setEventAttachLog(result);
	}
	return result;
}

