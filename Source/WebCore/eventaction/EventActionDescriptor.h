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

#ifndef EventActionDescriptor_h
#define EventActionDescriptor_h

#include <string>

namespace WebCore {

    /**
     * An EventActionDescriptor represents a concrete event action to be executed in the system.
     *
     * Each descriptor contains a unique ID, a name string and a params string.
     *
     * ID: Used to uniquely identify this specific event action such that we can build an happens before graph.
     * We can't use the name since we do not make any guarantees that the name is unique.
     *
     * The ID is assigned sequentially in one execution. For this reason it is often not feasible to correlate IDs
     * across executions if reordering is used.
     *
     * // TODO(WebERA): It would be nice if we could use the name as a unique identifier and remove the ID
     *
     * Name: A name identifying the event action (not unique). The convention is write the name as follows:
     *
     * EVENTACTIONTYPE(arg1, arg2, arg3, arg4)
     *
     * We do some inspection of this string to do special casing.
     *
     * // TODO(WebERA) should we refactor the descriptor to remove the name and operate directly on its components?
     *
     * Params: Optional params. Not used at the moment.
     *
     * // TODO(WebERA) Remove params as part of the above TODO. We could just do proper matching on the descriptor.
     */
    class EventActionDescriptor {
    public:
        EventActionDescriptor(unsigned long id, const std::string& name);
        EventActionDescriptor(unsigned long id, const std::string& name, const std::string& params);
        EventActionDescriptor(const std::string& name, const std::string& params);
        EventActionDescriptor();

        bool isNull() const { return m_isNull; }

        const char* getName() const;
        const char* getParams() const;

        unsigned long getId() const { return m_id; }

        // Inspecting the name
        std::string getType() const;
        std::string getParameter(unsigned int number) const;

        bool operator==(const EventActionDescriptor& other) const;
        bool fuzzyCompare(const EventActionDescriptor& other) const;

        static EventActionDescriptor null;

    private:
        unsigned long m_id;

        std::string m_name;
        std::string m_params;

        bool m_isNull;
    };

}

#endif
