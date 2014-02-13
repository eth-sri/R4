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

#include <QDataStream>

#include "network.h"

QNetworkReplyControllableRecord::QNetworkReplyControllableRecord(QNetworkReply* reply, QNetworkReplyControllableFactoryRecord* factory, QObject* parent)
    : QNetworkReplyControllableLive(reply, parent)
    , m_factory(factory)
{
}

QNetworkReplyControllableRecord::~QNetworkReplyControllableRecord()
{
    m_factory->controllableDone(this);
}

QNetworkReplyControllableFactoryRecord::QNetworkReplyControllableFactoryRecord(QString logFile)
    : QNetworkReplyControllableFactory()
    , m_fp(new QFile(logFile))
    , m_doneCounter(0)
{
    m_fp->open(QIODevice::WriteOnly);
    ASSERT(m_fp->isOpen());
}

QNetworkReplyControllableFactoryRecord::~QNetworkReplyControllableFactoryRecord()
{
    writeNetworkFile();
}

void QNetworkReplyControllableFactoryRecord::writeNetworkFile()
{
    if (m_fp->isOpen()) {

        std::set<QNetworkReplyControllableRecord*>::iterator iter = m_openNetworkSessions.begin();
        while (iter != m_openNetworkSessions.end()) {
            (*iter)->getSnapshot()->serialize(m_fp);
            iter++;
        }

        m_fp->close();
    }
}

void QNetworkReplyControllableFactoryRecord::controllableDone(QNetworkReplyControllableRecord* controllable)
{
    if (m_fp->isOpen()) {
        controllable->getSnapshot()->serialize(m_fp);
    }

    m_openNetworkSessions.erase(controllable);
    m_doneCounter++;
}
