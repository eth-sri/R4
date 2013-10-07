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

#include <climits>

#include "EventActionDescriptor.h"

namespace WebCore {

StringSet EventActionDescriptor::m_strings;
EventActionDescriptor EventActionDescriptor::null;

EventActionDescriptor::EventActionDescriptor(unsigned long id, const std::string& name)
    : m_id(id)
    , m_nameIndex(EventActionDescriptor::m_strings.addString(name.c_str()))
    , m_paramsIndex(EventActionDescriptor::m_strings.addString(""))
    , m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor(unsigned long id, const std::string& name, const std::string& params)
    : m_id(id)
    , m_nameIndex(EventActionDescriptor::m_strings.addString(name.c_str()))
    , m_paramsIndex(EventActionDescriptor::m_strings.addString(params.c_str()))
    , m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor(const std::string& name, const std::string& params)
    : m_id(UINT_MAX)
    , m_nameIndex(EventActionDescriptor::m_strings.addString(name.c_str()))
    , m_paramsIndex(EventActionDescriptor::m_strings.addString(params.c_str()))
    , m_isNull(false)
{
}

EventActionDescriptor::EventActionDescriptor()
    : m_id(0)
    , m_nameIndex(-1)
    , m_paramsIndex(-1)
    , m_isNull(true)
{
}

bool EventActionDescriptor::operator==(const EventActionDescriptor &other) const
{
    return m_id == other.m_id && m_nameIndex == other.m_nameIndex && m_paramsIndex == other.m_paramsIndex;
}

bool EventActionDescriptor::fuzzyCompare(const EventActionDescriptor &other) const
{
    return m_nameIndex == other.m_nameIndex && m_paramsIndex == other.m_paramsIndex;
}

std::string EventActionDescriptor::getName() const
{
    return EventActionDescriptor::m_strings.getString(m_nameIndex);
}

std::string EventActionDescriptor::getParams() const
{
    return EventActionDescriptor::m_strings.getString(m_paramsIndex);
}

}
