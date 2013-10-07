/*
 * EventActionRegister.h
 *
 *  Created on: Oct 1, 2013
 *      Author: veselin
 */

#ifndef EVENTACTIONREGISTER_H_
#define EVENTACTIONREGISTER_H_

#include "eventaction/EventActionDescriptor.h"
#include "eventaction/EventActionSchedule.h"

namespace WebCore {

typedef bool (*EventActionProviderFunction)(void* object, const std::string& params);

class EventActionRegisterMaps;

class EventActionRegister {
    WTF_MAKE_NONCOPYABLE(EventActionRegister);

public:
    EventActionRegister();
    virtual ~EventActionRegister();

    // Registration of event action providers able to handle an even action with the name <name>
    void registerEventActionProvider(void* object, const char* name, EventActionProviderFunction f);

    void updateActionProviderName(void* object, const char* name);
    void unregisterActionProvider(void* object);

    // Attempts to run execute an event action with the given name and parameters. Returns true on success.
    bool runEventAction(const EventActionDescriptor& descriptor);

    const EventActionDescriptor& currentEventActionDispatching() const
    {
        if (m_isDispatching) {
            return m_dispatchHistory->isEmpty() ? EventActionDescriptor::null : m_dispatchHistory->last();
        }

        return EventActionDescriptor::null;
    }

    EventActionDescriptor allocateEventDescriptor(const std::string& name, const std::string& param = "");

    EventActionSchedule* dispatchHistory() { return m_dispatchHistory; }

    void debugPrintNames() const;

private:

    void eventActionDispatchStart(const EventActionDescriptor& descriptor)
    {
        ASSERT(!m_isDispatching);

        m_dispatchHistory->append(descriptor);
        m_isDispatching = true;
    }

    void eventActionDispatchEnd()
    {
        ASSERT(m_isDispatching);

        m_isDispatching = false;
    }

    EventActionRegisterMaps* m_maps;
    bool m_isDispatching;
    unsigned long m_nextEventActionDescriptorId;

    EventActionSchedule* m_dispatchHistory;
};

}  // namespace WebCore

#endif /* EVENTACTIONREGISTER_H_ */
