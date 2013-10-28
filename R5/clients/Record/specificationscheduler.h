/*
 * All rights reserved.
 *
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

#include <QObject>

#include <queue>

#include <wtf/ExportMacros.h>
#include <WebCore/platform/Timer.h>
#include <WebCore/platform/schedule/Scheduler.h>

#include "eventaction/EventActionSchedule.h"
#include "eventaction/EventActionDescriptor.h"

#ifndef SPECIFICATION_SCHEDULER_H
#define SPECIFICATION_SCHEDULER_H

class SpecificationScheduler : public QObject, public WebCore::Scheduler
{
    Q_OBJECT

public:
    SpecificationScheduler();
    ~SpecificationScheduler();

    void eventActionScheduled(const WebCore::EventActionDescriptor& descriptor, WebCore::EventActionRegister* eventActionRegister);
    void executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister);

private:
    std::queue<WebCore::EventActionDescriptor> m_parsingQueue;
    std::queue<WebCore::EventActionDescriptor> m_networkQueue;
    std::queue<WebCore::EventActionDescriptor> m_otherQueue;

};

#endif // SPECIFICATION_SCHEDULER_H
