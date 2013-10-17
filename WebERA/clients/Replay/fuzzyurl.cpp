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

#include "fuzzyurl.h"

FuzzyUrlMatcher::FuzzyUrlMatcher(const QUrl& url)
    : m_url(url)
{
}

unsigned int FuzzyUrlMatcher::score(const QUrl& other)
{
    // fastpath, the urls matches 100%
    if (m_url == other) {
        return MATCH;
    }

    // Reject domain mismatches
    if (m_url.host() != other.host()) {
        return MISMATCH;
    }

    // Reject path mismatches
    if (m_url.path() != other.path()) {
        return MISMATCH;
    }

    // Reject query mismatches
    if (m_url.hasQuery() != other.hasQuery()) {
        return MISMATCH;
    }

    if (m_url.queryItems().size() != other.queryItems().size()) {
        return MISMATCH;
    }

    // Reject fragment mismatch

    if (m_url.hasFragment() != other.hasFragment()) {
        return MISMATCH;
    }

    // Calculate score

    unsigned int score = 1; // give it a score of 1 because the domain, path, fragment and query length matches

    QPair<QString, QString> item;
    foreach (item, m_url.queryItems()) {
        if (!other.hasQueryItem(item.first)) {
            return MISMATCH; // query keys mismatch
        }

        if (item.second == other.queryItemValue(item.first)) {
            score++;
        }
    }

    return score;
}
