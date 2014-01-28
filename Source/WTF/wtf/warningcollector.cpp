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

#include "warningcollector.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <assert.h>
#include <malloc.h>
#include <sstream>

namespace WTF {

void WarningCollector::collect(const std::string& module, const std::string& shortDescription, const std::string& details)
{
    m_warnings.push_back(Warning(module, shortDescription, details));
}

void WarningCollector::writeLogFile(const std::string& filepath)
{
    std::ofstream fp;
    fp.open(filepath.c_str(), std::ios_base::out);

    assert(fp.is_open());

    std::list<Warning>::iterator iter;
    for (iter = m_warnings.begin(); iter != m_warnings.end(); iter++) {

        std::string details = ((*iter).details);

        fp << (*iter).module << ";" << (*iter).shortDescription << ";" << (details.length() == 0 ? 0 : details.length() + 1) << std::endl;

        if (details.length() != 0) {
            fp << details << std::endl;
        }
    }

    fp.close();
}

WarningCollector WarningCollector::readLogFile(const std::string& filepath)
{
    WarningCollector collector;

    std::ifstream fp;
    fp.open(filepath.c_str(), std::ios_base::in);

    assert(fp.is_open());

    while (fp.good()) {

        // Read the entire line and check for the blank line (indicating EOF)

        std::string line;
        std::getline(fp, line);

        if (line.compare("") == 0) {
            break; // the last line is blank
        }

        // Parse header of warning

        std::stringstream warning(line);

        std::string module;
        std::getline(warning, module, ';');

        std::string shortDescription;
        std::getline(warning, shortDescription, ';');

        std::string detailsLength;
        std::getline(warning, detailsLength);

        // If we have details on the next line, then include those

        int length = atoi(detailsLength.c_str());

        if (length == 0) {
            collector.m_warnings.push_back(Warning(module, shortDescription, ""));

        } else {
            char* details = new char [length];
            fp.read(details, length);

            collector.m_warnings.push_back(Warning(module, shortDescription, details));

            delete details;
        }

    }

    fp.close();

    return collector;
}

}
