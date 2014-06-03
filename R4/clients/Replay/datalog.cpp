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

#include <iostream>

#include <QFile>
#include <QDataStream>

#include <WebCore/platform/ThreadTimers.h>
#include <WebCore/platform/ThreadGlobalData.h>

#include <wtf/warningcollectorreport.h>

#include "datalog.h"

TimeProviderReplay::TimeProviderReplay(QString logPath)
    : TimeProviderBase()
    , m_mode(STRICT)
{
    deserialize(logPath);
}

double TimeProviderReplay::currentTime()
{

    double time = JSC::TimeProviderDefault::currentTime();

    if (m_mode == STOP) {
        logTimeAccess(time);
        return time;
    }

    if (m_currentDescriptorString.isNull()) {
        std::cerr << "Error: Time requested by non-schedulable event action." << std::endl;
        std::exit(1);

        logTimeAccess(time);
        return time;
    }

    Log::iterator iter = m_in_log.find(m_currentDescriptorString);

    if (iter == m_in_log.end() || iter->isEmpty()) {

        if (m_mode == BEST_EFFORT || m_mode == BEST_EFFORT_NOND) {
            WTF::WarningCollectorReport("WEBERA_TIME_DATA", "New access to the time API in best effort mode.", "");

            logTimeAccess(time);
            return time;
        }

        std::cerr << "Error: Time requested but time log is empty (exact mode)." << std::endl;
        std::exit(1);

        logTimeAccess(time);
        return time;
    }

    time = iter->takeFirst();
    logTimeAccess(time);
    return time;
}

void TimeProviderReplay::deserialize(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::ReadOnly);

    ASSERT(fp.isOpen());

    QDataStream in(&fp);
    in >> m_in_log;

    fp.close();
}

RandomProviderReplay::RandomProviderReplay(QString logPath)
    : RandomProviderBase()
    , m_mode(STRICT)
{
    deserialize(logPath);
}

double RandomProviderReplay::get()
{

    double random = JSC::RandomProviderDefault::get();

    if (m_mode == STOP) {
        logRandomAccess(random);
        return random;
    }

    if (m_currentDescriptorString.isNull()) {
        std::cerr << "Error: Random number requested by non-schedulable event action." << std::endl;
        std::exit(1);

        logRandomAccess(random);
        return random;
    }

    DLog::iterator iter = m_in_double_log.find(m_currentDescriptorString);

    if (iter == m_in_double_log.end() || iter->isEmpty()) {

        if (m_mode == BEST_EFFORT || m_mode == BEST_EFFORT_NOND) {
            WTF::WarningCollectorReport("WEBERA_RANDOM_DATA", "New access to the random API in best effort mode.", "");
            logRandomAccess(random);
            return random;
        }

        std::cerr << "Error: Random number requested but random log is empty (exact mode)." << std::endl;
        std::exit(1);

        logRandomAccess(random);
        return random;
    }

    random = iter->takeFirst();
    logRandomAccess(random);
    return random;
}

unsigned RandomProviderReplay::getUint32()
{

    unsigned random = JSC::RandomProviderDefault::getUint32();

    if (m_mode == STOP) {
        logRandomAccessUint32(random);
        return random;
    }

    if (m_currentDescriptorString.isNull()) {
        std::cerr << "Error: Random number requested by non-schedulable event action." << std::endl;
        std::exit(1);

        logRandomAccessUint32(random);
        return random;
    }

    ULog::iterator iter = m_in_unsigned_log.find(m_currentDescriptorString);

    if (iter == m_in_unsigned_log.end() || iter->isEmpty()) {

        if (m_mode == BEST_EFFORT || m_mode == BEST_EFFORT_NOND) {
            WTF::WarningCollectorReport("WEBERA_RANDOM_DATA", "New access to the random API in best effort mode.", "");
            logRandomAccessUint32(random);
            return random;
        }

        std::cerr << "Error: Random number requested but random log is empty (exact mode)." << std::endl;
        std::exit(1);

        logRandomAccessUint32(random);
        return random;
    }

    random = iter->takeFirst();
    logRandomAccessUint32(random);
    return random;
}

void RandomProviderReplay::deserialize(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::ReadOnly);

    ASSERT(fp.isOpen());

    QDataStream in(&fp);
    in >> m_in_double_log;
    in >> m_in_unsigned_log;

    fp.close();
}
