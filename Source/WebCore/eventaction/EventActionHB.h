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

#ifndef EventActionHB_h
#define EventActionHB_h

#include <ostream>
#include <istream>

#include <wtf/Noncopyable.h>
#include <wtf/ExportMacros.h>
#include <wtf/Vector.h>

#include "EventActionDescriptor.h"

namespace WebCore {

    // Happens before graph for event actions.
    class EventActionsHB {
        WTF_MAKE_NONCOPYABLE(EventActionsHB);

    public:
        EventActionsHB();
        ~EventActionsHB();

        void addExplicitArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later);
        void addTimedArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later, double duration);
        void addTimedArc(const EventActionDescriptor& earlier, const EventActionDescriptor& later, int duration);

        void serialize(std::ostream& stream) const;
        static EventActionsHB* deserialize(std::istream& stream);

    private:

        struct Arc {
            Arc(const EventActionDescriptor& from, const EventActionDescriptor to, int duration)
                : from(from)
                , to(to)
                , duration(duration)
            {}

            const EventActionDescriptor from;
            const EventActionDescriptor to;
            const int duration;
        };

        WTF::Vector<const Arc*> m_arcs;

    };

}

#endif
