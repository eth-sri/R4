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

#include <assert.h>
#include <climits>

#include "EventActionDescriptor.h"

namespace WebCore {

EventActionDescriptor EventActionDescriptor::null;

EventActionDescriptor::EventActionDescriptor(unsigned long id, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor(unsigned long id, const std::string& name, const std::string& params)
    : m_id(id)
	, m_name(name)
	, m_params(params)
	, m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor(const std::string& name, const std::string& params)
    : m_id(UINT_MAX)
	, m_name(name)
	, m_params(params)
    , m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor()
    : m_id(0)
    , m_isNull(true)
{
}

bool EventActionDescriptor::operator==(const EventActionDescriptor& other) const
{
    return m_id == other.m_id && m_name == other.m_name && m_params == other.m_params;
}

bool EventActionDescriptor::fuzzyCompare(const EventActionDescriptor &other) const
{
    return m_name == other.m_name && m_params == other.m_params;
}

const char* EventActionDescriptor::getName() const
{
    return m_name.c_str();
}

const char* EventActionDescriptor::getParams() const
{
    return m_params.c_str();
}

std::string EventActionDescriptor::getType() const
{
    size_t typeEnd = m_name.find('(');

    if (typeEnd == std::string::npos) {
        return std::string();
    }

    return m_name.substr(0, typeEnd);
}

std::string EventActionDescriptor::getParameter(unsigned int number) const
{
    size_t start;
    size_t end = m_name.find('(');

    assert(end != std::string::npos);

    for (int i = 0; i <= number; i++) {
        start = end;
        end = m_name.find(',', start);

        if (end == std::string::npos) {
            end = m_name.find(')', start);
        }

        assert(end != std::string::npos);
    }

    return m_name.substr(start+1, end-start-1);
}

}
