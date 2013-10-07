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

#include "EventActionHB.h"

namespace WebCore {

EventActionsHB::EventActionsHB()
{
}

EventActionsHB::~EventActionsHB()
{
    // free archs
    for (WTF::Vector<const Arc*>::iterator it = m_arcs.begin(); it != m_arcs.end(); it++) {
        delete *it;
    }
}

void EventActionsHB::addExplicitArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later) {
    if (earlier == later) {
        CRASH();
    }

    m_arcs.append(new Arc(earlier, later, -1));
}

void EventActionsHB::addTimedArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later, double duration) {
    if (earlier == later) {
        CRASH();
    }

    m_arcs.append(new Arc(earlier, later, duration * 1000));
}

void EventActionsHB::addTimedArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later, int duration) {
    if (earlier == later) {
        CRASH();
    }

    m_arcs.append(new Arc(earlier, later, duration));
}

bool EventActionsHB::haveAnyOrderRelation(const EventActionDescriptor& ea1, const EventActionDescriptor& ea2) const
{
    for (WTF::Vector<const Arc*>::const_iterator it = m_arcs.begin(); it != m_arcs.end(); it++) {
        if (((*it)->to == ea1 && (*it)->from == ea2) ||
              ((*it)->to == ea2 && (*it)->from == ea1)) {
            return true;
        }
    }

    return false;
}

void EventActionsHB::serialize(std::ostream& stream) const
{
    for (WTF::Vector<const Arc*>::const_iterator it = m_arcs.begin(); it != m_arcs.end(); it++) {
        if (!(*it)->from.isNull()) {
            stream << (*it)->from.getId() << ";" << (*it)->from.getName() << ";";
            stream << (*it)->to.getId() << ";" << (*it)->to.getName() << ";";
            stream << (*it)->duration << std::endl;
        }
    }
}

EventActionsHB* EventActionsHB::deserialize(std::istream& stream)
{
    // TODO(WebERA): This function does very little input validation,
    // we should add a bit just to give the user useful feedback if something is illformed

    EventActionsHB* hb = new EventActionsHB();

    while (stream.good()) {
        std::string arc;
        std::getline(stream, arc);

        if (arc.compare("") == 0) {
            continue; // ignore blank lines
        }

        std::stringstream arcStream(arc);

        std::string fromId;
        std::getline(arcStream, fromId, ';');

        std::string fromDescription;
        std::getline(arcStream, fromDescription, ';');

        std::string toId;
        std::getline(arcStream, toId, ';');

        std::string toDescription;
        std::getline(arcStream, toDescription, ';');

        std::string duration;
        std::getline(arcStream, duration);

        EventActionDescriptor from(strtoul(fromId.c_str(), NULL, 10), fromDescription);
        EventActionDescriptor to(strtoul(toId.c_str(), NULL, 10), toDescription);

        int durationInt = atoi(duration.c_str());

        if (durationInt == -1) {
            hb->addExplicitArc(from, to);
        } else {
            hb->addTimedArc(from, to, durationInt);
        }
    }

    return hb;
}


}
