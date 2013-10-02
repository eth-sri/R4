/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <sstream>
#include <cstdlib>

#include "EventActionSchedule.h"

namespace WebCore {

EventActionSchedule::EventActionSchedule()
    : m_nextEventActionDescriptorId(0)
    , m_isDispatching(false)
{
}

EventActionSchedule::EventActionSchedule(const WTF::Vector<EventActionDescriptor>& schedule)
    : m_schedule(schedule)
    , m_nextEventActionDescriptorId(0)
    , m_isDispatching(false)
{
}

EventActionSchedule::~EventActionSchedule()
{
}

EventActionDescriptor EventActionSchedule::allocateEventDescriptor(const std::string& description)
{
    return EventActionDescriptor(m_nextEventActionDescriptorId++, description);
}

void EventActionSchedule::serialize(std::ostream& stream) const
{
    stream << m_nextEventActionDescriptorId << std::endl;

    for (WTF::Vector<EventActionDescriptor>::const_iterator it = m_schedule.begin(); it != m_schedule.end(); it++) {
        stream << (*it).getId() << ";" << (*it).getDescription() << std::endl;
    }
}

EventActionSchedule* EventActionSchedule::deserialize(std::istream& stream)
{
    // TODO(WebERA): This function does very little input validation,
    // we should add a bit just to give the user useful feedback if something is illformed

    EventActionSchedule* schedule = new EventActionSchedule();

    std::string nextEventActionDescriptorId;
    std::getline(stream, nextEventActionDescriptorId);

    schedule->m_nextEventActionDescriptorId = strtoul(nextEventActionDescriptorId.c_str(), NULL, 10);

    while (stream.good()) {
        std::string eventaction;
        std::getline(stream, eventaction);

        if (eventaction.compare("") == 0) {
            continue; // ignore blank lines
        }

        std::stringstream eventactionStream(eventaction);

        std::string id;
        std::getline(eventactionStream, id, ';');

        std::string description;
        std::getline(eventactionStream, description);

        EventActionDescriptor descriptor(strtoul(id.c_str(), NULL, 10), description);

        schedule->m_schedule.append(descriptor);
    }

    return schedule;
}

}
