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
    : WTF::Vector<EventActionScheduleItem>()
{
}

void EventActionSchedule::serialize(std::ostream& stream) const
{
    for (EventActionSchedule::const_iterator it = this->begin(); it != this->end(); it++) {
        stream << (*it).first << ";" << (*it).second.serialize() << std::endl;
    }
}

EventActionSchedule* EventActionSchedule::deserialize(std::istream& stream)
{
    EventActionSchedule* schedule = new EventActionSchedule();

    while (stream.good()) {
        std::string eventaction;
        std::getline(stream, eventaction);

        if (eventaction.compare("") == 0) {
            continue; // ignore blank lines
        }

        if (eventaction.compare("<relax>") == 0 || eventaction.compare("<change>") == 0) {
            schedule->append(std::pair<WTF::EventActionId, WTF::EventActionDescriptor>(0, WTF::EventActionDescriptor::null));
            continue;
        }

        std::stringstream eventactionStream(eventaction);

        std::string id;
        std::getline(eventactionStream, id, ';');

        std::string description;
        std::getline(eventactionStream, description);

        WTF::EventActionDescriptor descriptor = WTF::EventActionDescriptor::deserialize(description);

        schedule->append(std::pair<WTF::EventActionId, WTF::EventActionDescriptor>(atoi(id.c_str()), descriptor));
    }

    return schedule;
}

}
