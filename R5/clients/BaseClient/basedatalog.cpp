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

#include "basedatalog.h"

void TimeProviderBase::logTimeAccess(double time)
{
    const WTF::EventActionDescriptor descriptor = WebCore::threadGlobalData().threadTimers().eventActionRegister()->currentEventActionDispatching();

    if (descriptor.isNull()) {
        return;
    }

    QString ident = QString::fromStdString(descriptor.toString());

    Log::iterator iter = m_log.find(ident);

    if (iter == m_log.end()) {
        LogEntries logEntries;
        logEntries.append(time);
        m_log.insert(ident, logEntries);

    } else {
        iter->append(time);
    }
}

void TimeProviderBase::attach()
{
    JSC::TimeProvider::setInstance(this);
}

void TimeProviderBase::writeLogFile(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::WriteOnly);

    ASSERT(fp.isOpen());

    QDataStream out(&fp);
    out << m_log;

    fp.close();
}

void RandomProviderBase::logRandomAccess(double random)
{

    const WTF::EventActionDescriptor descriptor = WebCore::threadGlobalData().threadTimers().eventActionRegister()->currentEventActionDispatching();

    if (descriptor.isNull()) {
        return;
    }

    QString ident = QString::fromStdString(descriptor.toString());

    DLog::iterator iter = m_double_log.find(ident);

    if (iter == m_double_log.end()) {
        DLogEntries logEntries;
        logEntries.append(random);
        m_double_log.insert(ident, logEntries);

    } else {
        iter->append(random);
    }
}

void RandomProviderBase::logRandomAccessUint32(unsigned random)
{

    const WTF::EventActionDescriptor descriptor = WebCore::threadGlobalData().threadTimers().eventActionRegister()->currentEventActionDispatching();

    if (descriptor.isNull()) {
        return;
    }

    QString ident = QString::fromStdString(descriptor.toString());

    ULog::iterator iter = m_unsigned_log.find(ident);

    if (iter == m_unsigned_log.end()) {
        ULogEntries logEntries;
        logEntries.append(random);
        m_unsigned_log.insert(ident, logEntries);

    } else {
        iter->append(random);
    }
}

void RandomProviderBase::attach()
{
    JSC::RandomProvider::setInstance(this);
}

void RandomProviderBase::writeLogFile(QString path)
{
    QFile fp(path);
    fp.open(QIODevice::WriteOnly);

    ASSERT(fp.isOpen());

    QDataStream out(&fp);
    out << m_double_log;
    out << m_unsigned_log;

    fp.close();
}

