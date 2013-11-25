/*
 * EventActionRegister.h
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#ifndef EVENTACTIONREGISTER_H_
#define EVENTACTIONREGISTER_H_

#include <vector>

#include "wtf/EventActionDescriptor.h"
#include "wtf/EventActionSchedule.h"

#include "wtf/ActionLogReport.h"

namespace WebCore {

typedef bool (*EventActionHandlerFunction)(void* object, const WTF::EventActionDescriptor& descriptor);

class EventActionRegisterMaps;

/**
 * The EventActionRegister maintains a register of event actions pending execution.
 *
 * A scheduler is used to decide when an event action is executed.
 *
 * Two types of handlers can be registered for event actions, event action handlers and event action providers.
 *
 * Event action handlers:
 *    A one-shot handler registered for one specific event action. Matches on event action type and params.
 *
 * Event action provider:
 *    A handler that gets the chance to handle any event action of a particular type before matching with
 *    event action handlers.
 *
 */
class EventActionRegister {
    WTF_MAKE_NONCOPYABLE(EventActionRegister);

public:
    EventActionRegister();
    virtual ~EventActionRegister();

    // Registration of event action providers and handlers
    void registerEventActionProvider(const std::string& type, EventActionHandlerFunction f, void* object);
    void registerEventActionHandler(const WTF::EventActionDescriptor& descriptor, EventActionHandlerFunction f, void* object);
    void deregisterEventActionHandler(const WTF::EventActionDescriptor& descriptor);

    // Attempts to execute an event action. Returns true on success.
    bool runEventAction(const WTF::EventActionDescriptor& descriptor);

    // TODO(WebERA): We should not use ghost operations anywere, it is a bit unclear how well we can replay them / reorder them

    // Ghost operations, used to simulate the execution of certain un-schedulable event actions
    void enterGhostEventAction(WTF::EventActionId id, ActionLog::EventActionType type);
    void exitGhostEventAction();

    // Immediate operations, used to mark the current executing timer as a schedulable event action
    // These are only feasible if combined with a event action provider for replay
    void enterImmediateEventAction(ActionLog::EventActionType type, const WTF::EventActionDescriptor& descriptor);
    void exitImmediateEventAction();

    // Used to inspect the currently executed event action globally
    const WTF::EventActionDescriptor& currentEventActionDispatching() const
    {
        if (m_isDispatching) {
            return m_dispatchHistory->isEmpty() ? WTF::EventActionDescriptor::null : m_dispatchHistory->last().second;
        }

        return WTF::EventActionDescriptor::null;
    }

    EventActionSchedule* dispatchHistory() { return m_dispatchHistory; }

    std::set<std::string> getWaitingNames();

    void debugPrintNames() const;

    ActionLog::EventActionType toActionLogType(WTF::EventActionCategory category) {

        // TODO(WebERA-HB-REVIEW): I have retained the old ActionLog types, but I don't know if we can just add in our slightly different types (or if these types are correct).

        switch (category) {
        case WTF::OTHER:
        case WTF::PARSING:
            return ActionLog::UNKNOWN;
            break;
        case WTF::TIMER:
            return ActionLog::TIMER;
            break;
        case WTF::USER_INTERFACE:
            return ActionLog::USER_INTERFACE;
            break;
        case WTF::NETWORK:
            return ActionLog::NETWORK;
            break;

        default:
            ASSERT_NOT_REACHED();
            return ActionLog::UNKNOWN;
        }
    }

private:

    void eventActionDispatchStart(WTF::EventActionId id, const WTF::EventActionDescriptor& descriptor)
    {
        ASSERT(!m_isDispatching);

        m_dispatchHistory->append(EventActionScheduleItem(id, descriptor));
        m_isDispatching = true;
    }

    void eventActionDispatchEnd(bool commit)
    {
        ASSERT(m_isDispatching);

        m_isDispatching = false;

        if (!commit) {
            m_dispatchHistory->removeLast();
        }
    }

    EventActionRegisterMaps* m_maps;
    bool m_isDispatching;

    EventActionSchedule* m_dispatchHistory;
};

}  // namespace WebCore

#endif /* EVENTACTIONREGISTER_H_ */
