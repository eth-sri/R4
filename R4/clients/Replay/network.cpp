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
#include <QFile>

#include "fuzzyurl.h"

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
        m_snapshots.insert(snapshot->getUrl().toString(), snapshot);
    }

    fp.close();
}

WebCore::QNetworkReplyControllable* QNetworkReplyControllableFactoryReplay::construct(QNetworkReply* reply, QObject* parent)
{
    SnapshotMap::const_iterator iter = m_snapshots.find(reply->url().toString());

    if (iter == m_snapshots.end()) {
        // Try fuzzy matching
        std::cout << "Warning: No exact match for URL (" << reply->url().toString().toStdString() << ") found, fuzzy matching" << std::endl;

        FuzzyUrlMatcher matcher(reply->url());

        unsigned int bestScore = 0;
        WebCore::QNetworkReplyInitialSnapshot* bestSnapshot = NULL;

        iter = m_snapshots.begin();
        for (; iter != m_snapshots.end(); iter++) {
            WebCore::QNetworkReplyInitialSnapshot* snapshot = (*iter);

            unsigned int score = matcher.score(snapshot->getUrl());

            if (score > bestScore) {
                bestScore = score;
                bestSnapshot = snapshot;
            }
        }

        if (bestSnapshot != NULL) {
            // We found a fuzzy match

            std::cout << "Fuzzy match found (" << bestSnapshot->getUrl().toString().toStdString() << ")" << std::endl;

            m_snapshots.remove(bestSnapshot->getUrl().toString(), bestSnapshot);
            return new QNetworkReplyControllableReplay(reply, bestSnapshot, parent);
        }

        std::cout << "Fuzzy match not found, using a live connection (best effort)" << std::endl;
        return new WebCore::QNetworkReplyControllableLive(reply, parent);
    }

    m_snapshots.remove((*iter)->getUrl().toString(), (*iter));
    return new QNetworkReplyControllableReplay(reply, (*iter), parent);
}
