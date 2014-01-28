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

#ifndef REPLAYSCHEDULER_H
#define REPLAYSCHEDULER_H

#include <string>
#include <ostream>

#include <QObject>
#include <QTimer>

#include <wtf/ExportMacros.h>
#include <wtf/Vector.h>

#include <WebCore/platform/Timer.h>
#include <WebCore/platform/schedule/Scheduler.h>

#include "wtf/EventActionSchedule.h"
#include "wtf/EventActionDescriptor.h"

#include "replaymode.h"
#include "datalog.h"
#include "network.h"

class ReplayScheduler : public QObject, public WebCore::Scheduler
{
    Q_OBJECT

public:
    ReplayScheduler(const std::string& schedulePath, QNetworkReplyControllableFactoryReplay* networkProvider, TimeProviderReplay* timeProvider, RandomProviderReplay* randomProvider);
    ~ReplayScheduler();

    void eventActionScheduled(const WTF::EventActionDescriptor& descriptor, WebCore::EventActionRegister* eventActionRegister);
    void eventActionDescheduled(const WTF::EventActionDescriptor&, WebCore::EventActionRegister*) {}

    void executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister);

    bool isFinished();

    void stop() {
        m_networkProvider->setMode(STOP);
        m_timeProvider->setMode(STOP);
        m_randomProvider->setMode(STOP);
        m_mode = STOP;

        emit sigDone();
    }

    bool wasReplaySuccessful() {
        return m_replaySuccessful;
    }

private:

    bool executeDelayedEventAction(WebCore::EventActionRegister* eventActionRegister);

    void debugPrintTimers(std::ostream& out, WebCore::EventActionRegister* eventActionRegister);

    WebCore::EventActionSchedule* m_schedule;

    QNetworkReplyControllableFactoryReplay* m_networkProvider;
    TimeProviderReplay* m_timeProvider;
    RandomProviderReplay* m_randomProvider;

    ReplayMode m_mode;

    bool m_replaySuccessful;

    QTimer m_eventActionTimeoutTimer;
    bool m_skipEventActionsUntilHit;

private slots:
    void slEventActionTimeout();

signals:
    void sigDone();
};

#endif // REPLAYSCHEDULER_H
