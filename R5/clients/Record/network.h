/*
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

#ifndef NETWORK_H
#define NETWORK_H

#include <QFile>
#include <set>

#include <JavaScriptCore/runtime/JSExportMacros.h>
#include <WebCore/platform/network/qt/QNetworkReplyHandler.h>

class QNetworkReplyControllableFactoryRecord;

class QNetworkReplyControllableRecord : public WebCore::QNetworkReplyControllableLive {
    Q_OBJECT

public:
    QNetworkReplyControllableRecord(QNetworkReply*, QNetworkReplyControllableFactoryRecord* factory, QObject* parent = 0);
    ~QNetworkReplyControllableRecord();

    WebCore::QNetworkReplyInitialSnapshot* getSnapshot() {
        return m_initialSnapshot;
    }

private:
    QNetworkReplyControllableFactoryRecord* m_factory;

};

class QNetworkReplyControllableFactoryRecord : public WebCore::QNetworkReplyControllableFactory
{

public:
    QNetworkReplyControllableFactoryRecord();
    ~QNetworkReplyControllableFactoryRecord();

    void controllableDone(QNetworkReplyControllableRecord* controllable);
    void writeNetworkFile();

    QNetworkReplyControllableRecord* construct(QNetworkReply* reply, QObject* parent=0) {
        QNetworkReplyControllableRecord* session = new QNetworkReplyControllableRecord(reply, this, parent);
        m_openNetworkSessions.insert(session);
        return session;
    }

    unsigned int doneCounter() const {
        return m_doneCounter;
    }

private:
    QFile* m_fp;
    unsigned int m_doneCounter;
    std::set<QNetworkReplyControllableRecord*> m_openNetworkSessions;
};

#endif // NETWORK_H
