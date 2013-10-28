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

#include "platform/schedule/EventActionRegister.h"

#include "specificationscheduler.h"

SpecificationScheduler::SpecificationScheduler()
    : QObject(NULL)
    , Scheduler()
{
}

SpecificationScheduler::~SpecificationScheduler()
{
}

void SpecificationScheduler::eventActionScheduled(const WebCore::EventActionDescriptor& descriptor,
                                                  WebCore::EventActionRegister*)
{
    if (strcmp(descriptor.getType(), "HTMLDocumentParser") == 0) {
        m_parsingQueue.push(descriptor);
    } else if (strcmp(descriptor.getType(), "NETWORK") == 0) {
        m_networkQueue.push(descriptor);
    } else {
        m_otherQueue.push(descriptor);
    }
}

void SpecificationScheduler::executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister)
{
    // TODO(WebERA)
    //
    // This is very close to the desired specification trace.
    //
    // However, we should prioritize network events dependent on by page parsing
    // higher than network events related to e.g. AJAX requests.
    //
    // E.g., if a page is split into two network packets. We handle the first network event
    // and begin to parse the page. On the page, we have a script that will initiate an Ajax request.
    // When we run out of fragments to parse, we will either trigger the ajax network event or the
    // network event with the rest of the page to be parsed, depending on the timing of these network
    // events.

    if (!m_parsingQueue.empty()) {
        if (eventActionRegister->runEventAction(m_parsingQueue.front())) {
            m_parsingQueue.pop();
        }

    } else if (!m_networkQueue.empty()) {
        if (eventActionRegister->runEventAction(m_networkQueue.front())) {
            m_networkQueue.pop();
        }

    } else if (!m_otherQueue.empty()) {
        if (eventActionRegister->runEventAction(m_otherQueue.front())) {
            m_otherQueue.pop();
        }
    }
}
