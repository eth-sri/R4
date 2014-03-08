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

#ifndef BASEDATALOG_H
#define BASEDATALOG_H

#include <QHash>
#include <QList>

#include "JavaScriptCore/runtime/timeprovider.h"
#include "JavaScriptCore/runtime/randomprovider.h"

class TimeProviderBase : public JSC::TimeProviderDefault {

public:
    TimeProviderBase()
        : JSC::TimeProviderDefault()
    {
    }

    void attach();

    void logTimeAccess(double value);
    void writeLogFile(QString path);

protected:
    typedef QList<double> LogEntries;
    typedef QHash<QString, LogEntries> Log;

private:
    Log m_log;
};

class RandomProviderBase : public JSC::RandomProviderDefault {

public:
    RandomProviderBase()
        : JSC::RandomProviderDefault()
    {
    }

    void attach();

    void logRandomAccess(double value);
    void logRandomAccessUint32(unsigned value);
    void writeLogFile(QString path);

protected:
    typedef QList<double> DLogEntries;
    typedef QHash<QString, DLogEntries> DLog;

private:
    DLog m_double_log;

protected:
    typedef QList<unsigned> ULogEntries;
    typedef QHash<QString, ULogEntries> ULog;

private:
    ULog m_unsigned_log;
};

#endif // BASEDATALOG_H
