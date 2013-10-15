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

#include "network.h"

#include <QDataStream>
#include <QFile>

#include "network.h"

QNetworkReplyControllableReplay::QNetworkReplyControllableReplay(QNetworkReply* reply, WebCore::QNetworkReplyInitialSnapshot* snapshot, QObject* parent)
    : QNetworkReplyControllable(reply, parent)
{
    m_initialSnapshot = snapshot;

    WebCore::QNetworkReplyInitialSnapshot::QNetworkReplySnapshotEntry entry;
    QList<WebCore::QNetworkReplyInitialSnapshot::QNetworkReplySnapshotEntry>* snapshots = snapshot->getSnapshots();

    entry = snapshots->takeFirst();
    ASSERT(entry.first == WebCore::QNetworkReplyInitialSnapshot::INITIAL);
    m_currentSnapshot = entry.second;

    while (!snapshots->empty()) {
        entry = snapshots->takeFirst();
        enqueueSnapshot(entry.first, entry.second);
    }

}

QNetworkReplyControllableFactoryReplay::QNetworkReplyControllableFactoryReplay()
    : QNetworkReplyControllableFactory()
{
    QFile fp(QString::fromAscii("/tmp/network.data"));
    fp.open(QIODevice::ReadOnly);

    while (!fp.atEnd()) {
        WebCore::QNetworkReplyInitialSnapshot* snapshot = WebCore::QNetworkReplyInitialSnapshot::deserialize(&fp);
        m_snapshots.insert(snapshot->getDescriptor(), snapshot);
    }

    fp.close();
}

WebCore::QNetworkReplyControllable* QNetworkReplyControllableFactoryReplay::construct(QNetworkReply* reply, QObject* parent)
{
    QString descriptor = WebCore::QNetworkReplyInitialSnapshot::getDescriptor(reply->url());

    typedef QHash<QString, WebCore::QNetworkReplyInitialSnapshot*> map;
    map::const_iterator iter = m_snapshots.find(descriptor);

    if (iter == m_snapshots.end()) {
        std::cout << "Warning: Replay of unknown url (" << descriptor.toStdString() << ") not possible" << std::endl;
        return new WebCore::QNetworkReplyControllableLive(reply, parent);
    }

    return new QNetworkReplyControllableReplay(reply, (*iter), parent);
}
