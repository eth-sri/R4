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

#ifndef WARNINGCOLLECTOR_H
#define WARNINGCOLLECTOR_H

#include "warningcollectorreport.h"

#include <list>

namespace WTF {

class WarningCollector
{
public:
    WarningCollector() {}
    void collect(const std::string& module, const std::string& shortDescription, const std::string& details);
    void writeLogFile(const std::string& filepath);

    static WarningCollector readLogFile(const std::string& filepath);

private:
    typedef struct warning_t {
        const std::string module;
        const std::string shortDescription;
        const std::string details;

        warning_t(const std::string& module, const std::string& shortDescription, const std::string& details)
            : module(module)
            , shortDescription(shortDescription)
            , details(details)
        {}

    } Warning;

    std::list<Warning> m_warnings;
};

}

#endif // WARNINGCOLLECTOR_H
