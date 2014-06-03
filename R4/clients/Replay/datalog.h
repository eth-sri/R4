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

#ifndef DATALOG_H
#define DATALOG_H

#include <limits>

#include <QHash>
#include <QList>

#include "basedatalog.h"

#include "replaymode.h"

class TimeProviderReplay : public TimeProviderBase {

public:
    TimeProviderReplay(QString logPath);

    double currentTime();

    void setCurrentDescriptorString(QString ident) {
        m_currentDescriptorString = ident;
    }

    void unsetCurrentDescriptorString() {
        m_currentDescriptorString = QString();
    }

    void setMode(ReplayMode value) {
        m_mode = value;
    }

private:
    void deserialize(QString logPath);

    Log m_in_log;

    ReplayMode m_mode;
    QString m_currentDescriptorString;
};

class RandomProviderReplay : public RandomProviderBase {

public:
    RandomProviderReplay(QString logPath);

    double get();
    unsigned getUint32();

    void setCurrentDescriptorString(QString ident) {
        m_currentDescriptorString = ident;
    }

    void unsetCurrentDescriptorString() {
        m_currentDescriptorString = QString();
    }

    void setMode(ReplayMode value) {
        m_mode = value;
    }

private:
    void deserialize(QString logPath);

    DLog m_in_double_log;
    ULog m_in_unsigned_log;

    ReplayMode m_mode;
    QString m_currentDescriptorString;
};

#endif // DATALOG_H
