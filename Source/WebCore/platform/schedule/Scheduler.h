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
 *
 */

#ifndef Scheduler_h
#define Scheduler_h

#include <wtf/ExportMacros.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#include "eventaction/EventActionDescriptor.h"

#include "EventActionRegister.h"

namespace WebCore {

    class Scheduler {
        WTF_MAKE_NONCOPYABLE(Scheduler);

    public:
        Scheduler();
        virtual ~Scheduler();

        // Notifies the scheduler that a new event action has been registered to the EventActionRegister by ThreadTimers
        // Some schedulers may not execute the event action immediately, but delay it instead.
        virtual void eventActionScheduled(const EventActionDescriptor& descriptor, EventActionRegister& eventActionRegister) = 0;

        // Ask the scheduler to execute any delayed tasks
        // Called at every tick, after scheduling any new event actions
        virtual void executeDelayedEventActions(EventActionRegister& eventActionRegister) = 0;
    };
}

#endif
