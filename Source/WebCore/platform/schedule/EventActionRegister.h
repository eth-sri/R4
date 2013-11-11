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

typedef bool (*EventActionHandlerFunction)(void* object, const EventActionDescriptor& descriptor);

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
    void registerEventActionHandler(const EventActionDescriptor& descriptor, EventActionHandlerFunction f, void* object);
    void deregisterEventActionHandler(const EventActionDescriptor& descriptor);

    // Attempts to execute an event action. Returns true on success.
    bool runEventAction(const EventActionDescriptor& descriptor);

    // Used to inspect the currently executed event action globally
    const EventActionDescriptor& currentEventActionDispatching() const
    {
        if (m_isDispatching) {
            return m_dispatchHistory->isEmpty() ? EventActionDescriptor::null : m_dispatchHistory->last().second;
        }

        return EventActionDescriptor::null;
    }

    EventActionSchedule* dispatchHistory() { return m_dispatchHistory; }

    std::vector<std::string> getWaitingNames();

    void debugPrintNames() const;

private:

    void eventActionDispatchStart(EventActionId id, const EventActionDescriptor& descriptor)
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
