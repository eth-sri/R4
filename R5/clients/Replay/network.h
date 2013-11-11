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

#ifndef NETWORK_H
#define NETWORK_H

#include <QMultiHash>

#include <JavaScriptCore/runtime/JSExportMacros.h>
#include <WebCore/platform/network/qt/QNetworkReplyHandler.h>

class QNetworkReplyControllableFactoryReplay;

class QNetworkReplyControllableReplay : public WebCore::QNetworkReplyControllable {
    Q_OBJECT

public:
    QNetworkReplyControllableReplay(QNetworkReply*, WebCore::QNetworkReplyInitialSnapshot*, QObject* parent = 0);

};


class QNetworkReplyControllableFactoryReplay : public WebCore::QNetworkReplyControllableFactory
{

public:
    QNetworkReplyControllableFactoryReplay();

    WebCore::QNetworkReplyControllable* construct(QNetworkReply* reply, QObject* parent=0);

    void stop() {
        m_stopped = true;
    }

private:
    typedef QList<WebCore::QNetworkReplyInitialSnapshot*> SnapshotList;
    typedef QHash<QString, SnapshotList*> SnapshotMap;
    SnapshotMap m_snapshots;
    bool m_stopped;
};

#endif // NETWORK_H
