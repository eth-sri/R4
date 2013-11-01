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

#include "datalog.h"

TimeProviderReplay::TimeProviderReplay(QString logPath)
    : JSC::TimeProviderDefault()
    , m_stopped(false)
{
    deserialize(logPath);
}

double TimeProviderReplay::currentTime()
{

    double time = JSC::TimeProviderDefault::currentTime();

    if (m_currentDescriptorString.isNull() || m_stopped) {
        ASSERT(false); // dont allow non-deterministic input to uncontrolled event actions
        return time;
    }

    Log::iterator iter = m_log.find(m_currentDescriptorString);

    ASSERT(iter != m_log.end());
    ASSERT(!iter->isEmpty());

    return iter->takeFirst();
}

void TimeProviderReplay::attach()
{
    JSC::TimeProvider::setInstance(this);
}

void TimeProviderReplay::deserialize(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::ReadOnly);

    ASSERT(fp.isOpen());

    QDataStream in(&fp);
    in >> m_log;

    fp.close();
}


RandomProviderReplay::RandomProviderReplay(QString logPath)
    : JSC::RandomProviderDefault()
    , m_stopped(false)
{
    deserialize(logPath);
}

double RandomProviderReplay::get()
{

    double random = JSC::RandomProviderDefault::get();

    if (m_currentDescriptorString.isNull() || m_stopped) {
        ASSERT(false); // dont allow non-deterministic input to uncontrolled event actions
        return random;
    }

    DLog::iterator iter = m_double_log.find(m_currentDescriptorString);

    ASSERT(iter != m_double_log.end());
    ASSERT(!iter->isEmpty());

    return iter->takeFirst();
}

unsigned RandomProviderReplay::getUint32()
{

    unsigned random = JSC::RandomProviderDefault::getUint32();

    if (m_currentDescriptorString.isNull() || m_stopped) {
        ASSERT(false); // dont allow non-deterministic input to uncontrolled event actions
        return random;
    }

    ULog::iterator iter = m_unsigned_log.find(m_currentDescriptorString);

    ASSERT(iter != m_unsigned_log.end());
    ASSERT(!iter->isEmpty());

    return iter->takeFirst();
}

void RandomProviderReplay::attach()
{
    JSC::RandomProvider::setInstance(this);
}

void RandomProviderReplay::deserialize(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::ReadOnly);

    ASSERT(fp.isOpen());

    QDataStream in(&fp);
    in >> m_double_log;
    in >> m_unsigned_log;

    fp.close();
}
