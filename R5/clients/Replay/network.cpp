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
    , m_stopped(false)
    , m_relaxedReplayMode(false)
{
    QFile fp(QString::fromAscii("/tmp/network.data"));
    fp.open(QIODevice::ReadOnly);

    while (!fp.atEnd()) {
        WebCore::QNetworkReplyInitialSnapshot* snapshot = WebCore::QNetworkReplyInitialSnapshot::deserialize(&fp);

        QString url = snapshot->getUrl().toString();

        if (m_snapshots.contains(url)) {

            SnapshotMap::iterator iter = m_snapshots.find(url);
            (*iter)->append(snapshot);

        } else {
            SnapshotList* list = new SnapshotList();
            list->append(snapshot);

            m_snapshots.insert(url, list);
        }

    }

    fp.close();
}

WebCore::QNetworkReplyControllable* QNetworkReplyControllableFactoryReplay::construct(QNetworkReply* reply, QObject* parent)
{
    if (m_stopped) {
        return new WebCore::QNetworkReplyControllableLive(reply, parent);
    }

    SnapshotMap::const_iterator iter = m_snapshots.find(reply->url().toString());

    if (iter != m_snapshots.end()) {
        // HIT

        ASSERT(!(*iter)->isEmpty());
        return new QNetworkReplyControllableReplay(reply, (*iter)->takeFirst(), parent);
    }

    // Fuzzy matching

    if (m_relaxedReplayMode) {

        std::cout << "Warning: No exact match for URL (" << reply->url().toString().toStdString() << ") found, fuzzy matching" << std::endl;

        FuzzyUrlMatcher matcher(reply->url());

        unsigned int bestScore = 0;
        WebCore::QNetworkReplyInitialSnapshot* bestSnapshot = NULL;
        SnapshotMap::const_iterator bestList;

        iter = m_snapshots.begin();
        for (; iter != m_snapshots.end(); iter++) {

            if ((*iter)->isEmpty()) {
                continue; // skip emtpy lists
            }

            WebCore::QNetworkReplyInitialSnapshot* snapshot = (*iter)->first();

            unsigned int score = matcher.score(snapshot->getUrl());

            if (score > bestScore) {
                bestScore = score;
                bestSnapshot = snapshot;
                bestList = iter;
            }
        }

        if (bestSnapshot != NULL) {
            // We found a fuzzy match

            std::cout << "Fuzzy match found (" << bestSnapshot->getUrl().toString().toStdString() << ")" << std::endl;

            (*bestList)->pop_front();
            return new QNetworkReplyControllableReplay(reply, bestSnapshot, parent);
        }

        std::cout << "Fuzzy match not found, using a live connection (relaxed mode)" << std::endl;
        return new WebCore::QNetworkReplyControllableLive(reply, parent);
    }

    std::cerr << "Error, unknown network request (" << reply->url().toString().toStdString() << ") in exact mode." << std::endl;
    CRASH();

}
