/*
 *
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

#ifndef DATALOG_H
#define DATALOG_H

#include <QHash>
#include <QList>

#include "JavaScriptCore/runtime/timeprovider.h"
#include "JavaScriptCore/runtime/randomprovider.h"

class TimeProviderRecord : public JSC::TimeProviderDefault {

public:
    TimeProviderRecord()
        : JSC::TimeProviderDefault()
    {
    }

    void attach();
    double currentTime();

    void writeLogFile(QString path);

private:
    typedef QList<double> LogEntries;
    typedef QHash<QString, LogEntries> Log;
    Log m_log;
};

class RandomProviderRecord : public JSC::RandomProviderDefault {

public:
    RandomProviderRecord()
        : JSC::RandomProviderDefault()
    {
    }

    void attach();

    double get();
    unsigned getUint32();

    void writeLogFile(QString path);

private:
    typedef QList<double> DLogEntries;
    typedef QHash<QString, DLogEntries> DLog;
    DLog m_double_log;

    typedef QList<unsigned> ULogEntries;
    typedef QHash<QString, ULogEntries> ULog;
    ULog m_unsigned_log;
};

#endif // DATALOG_H
