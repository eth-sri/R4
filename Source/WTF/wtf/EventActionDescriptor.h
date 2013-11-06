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
     * Type: A type identifying the event action
     * Params: Parameters on the format "arg1,arg2,arg3,arg4" describing the event action
     *
     * We do inspect the parameters at times to do special casing.
     *
     */
    class EventActionDescriptor {
    public:
        EventActionDescriptor(const std::string& type, const std::string& params);
        EventActionDescriptor();

        bool isNull() const { return m_isNull; }

        const char* getType() const { return m_type.c_str(); }
        const char* getParams() const { return m_params.c_str(); }

        // Inspecting the params
        std::string getParameter(unsigned int number) const; // TODO this is a bit of a hack

        bool operator==(const EventActionDescriptor& other) const;
        bool operator!=(const EventActionDescriptor& other) const;

        std::string toString() const;

        std::string serialize() const;
        static EventActionDescriptor deserialize(const std::string&);

        static EventActionDescriptor null;

        static std::string escapeParam(const std::string& param);

    private:
        std::string m_type;
        std::string m_params;

        bool m_isNull;
    };

}

#endif
