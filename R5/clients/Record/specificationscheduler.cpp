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

#include <limits>
#include <cstdlib>
#include <iostream>

#include "platform/schedule/EventActionRegister.h"

#include "specificationscheduler.h"

SpecificationScheduler::SpecificationScheduler(QNetworkReplyControllableFactoryRecord* network)
    : QObject(NULL)
    , Scheduler()
    , m_network(network)
    , m_stopped(false)
{
}

SpecificationScheduler::~SpecificationScheduler()
{
}

void SpecificationScheduler::eventActionScheduled(const WTF::EventActionDescriptor& descriptor,
                                                  WebCore::EventActionRegister*)
{
    if (strcmp(descriptor.getType(), "HTMLDocumentParser") == 0) {
        m_parsingQueue.push(descriptor);
    } else if (strcmp(descriptor.getType(), "Network") == 0) {

        std::string networkSeqId = getNetworkSequenceId(descriptor);

        if (m_activeNetworkEvents.find(networkSeqId) == m_activeNetworkEvents.end()) {
            m_networkQueue.push(descriptor);
        } else {
            m_activeNetworkQueue.push(descriptor);
        }

    } else {
        m_otherQueue.push(descriptor);
    }
}

void dequeueSpecificElement(const WTF::EventActionDescriptor& descriptor,
                            std::queue<WTF::EventActionDescriptor>* queue)
{
    std::queue<WTF::EventActionDescriptor> old;
    std::swap(*queue, old);

    while (!old.empty()) {
        if (old.front() != descriptor) {
            queue->push(old.front());
        }

        old.pop();
    }
}

void SpecificationScheduler::eventActionDescheduled(const WTF::EventActionDescriptor& descriptor,
                                                    WebCore::EventActionRegister*)
{

    if (strcmp(descriptor.getType(), "HTMLDocumentParser") == 0) {

        dequeueSpecificElement(descriptor, &m_parsingQueue);

    } else if (strcmp(descriptor.getType(), "NETWORK") == 0) {

        std::string networkSeqId = getNetworkSequenceId(descriptor);

        if (m_activeNetworkEvents.find(networkSeqId) == m_activeNetworkEvents.end()) {
            dequeueSpecificElement(descriptor, &m_networkQueue);
        } else {
            dequeueSpecificElement(descriptor, &m_activeNetworkQueue);
        }

    } else {
        dequeueSpecificElement(descriptor, &m_otherQueue);
    }

}

void SpecificationScheduler::executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister)
{
    if (m_stopped) {
        return;
    }

    // Force ongoing network events to finish before anything else
    if (!m_activeNetworkEvents.empty()) {

        if (!m_activeNetworkQueue.empty()) {

            WTF::EventActionDescriptor descriptor = m_activeNetworkQueue.front();
            unsigned int currentFinishedNetworkJobs = m_network->doneCounter();

            if (eventActionRegister->runEventAction(descriptor)) {
                m_activeNetworkQueue.pop();

                if (strtoul(descriptor.getParameter(2).c_str(), NULL, 0) == ULONG_MAX ||
                        m_network->doneCounter() > currentFinishedNetworkJobs) {
                    // this was the last element in the sequence, remove it from the active events

                    std::string id = getNetworkSequenceId(descriptor);
                    m_activeNetworkEvents.erase(id);
                }
            }

        }

        return;
    }

    if (!m_parsingQueue.empty()) {

        if (eventActionRegister->runEventAction(m_parsingQueue.front())) {
            m_parsingQueue.pop();
        }

    } else if (!m_networkQueue.empty()) {

        WTF::EventActionDescriptor descriptor = m_networkQueue.front();
        unsigned int currentFinishedNetworkJobs = m_network->doneCounter();

        if (eventActionRegister->runEventAction(descriptor)) {

            m_networkQueue.pop();

            if (strtoul(descriptor.getParameter(2).c_str(), NULL, 0) != ULONG_MAX &&
                    currentFinishedNetworkJobs == m_network->doneCounter()) {
                // this is not the last element in the sequence

                std::string id = getNetworkSequenceId(descriptor);

                // add to active event actions
                m_activeNetworkEvents.insert(id);

                // split the exiting network event queue into active and non-active events
                std::queue<WTF::EventActionDescriptor> currentQueue;
                std::swap(currentQueue, m_networkQueue);

                while (!currentQueue.empty()) {
                    WTF::EventActionDescriptor existingEvent = currentQueue.front();
                    currentQueue.pop();

                    std::string existingEventId = getNetworkSequenceId(existingEvent); // url,url-sequence-number

                    if (id == existingEventId) {
                        m_activeNetworkQueue.push(existingEvent);
                    } else {
                        m_networkQueue.push(existingEvent);
                    }
                }
            }
        }

    } else if (!m_otherQueue.empty()) {
        if (eventActionRegister->runEventAction(m_otherQueue.front())) {
            m_otherQueue.pop();
        }
    }
}
