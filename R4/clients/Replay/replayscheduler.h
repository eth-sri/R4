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

#include "platform/schedule/EventActionRegister.h"
#include "wtf/ActionLogReport.h"
#include "wtf/warningcollectorreport.h"
#include "wtf/EventActionSchedule.h"
#include "wtf/EventActionDescriptor.h"

#include "replaymode.h"
#include "datalog.h"
#include "network.h"

enum ReplaySchedulerState {
    RUNNING, TIMEOUT, FINISHED, ERROR
};

class ReplayScheduler : public QObject, public WebCore::Scheduler
{
    Q_OBJECT

public:
    ReplayScheduler(const std::string& schedulePath, QNetworkReplyControllableFactoryReplay* networkProvider, TimeProviderReplay* timeProvider, RandomProviderReplay* randomProvider);
    ~ReplayScheduler();

    void eventActionScheduled(const WTF::EventActionDescriptor& descriptor, WebCore::EventActionRegister* eventActionRegister);
    void eventActionDescheduled(const WTF::EventActionDescriptor&, WebCore::EventActionRegister*) {}

    void executeDelayedEventActions(WebCore::EventActionRegister* eventActionRegister);
    bool tryExecuteEventActionDescriptor(
            WebCore::EventActionRegister* eventActionRegister,
            const WebCore::EventActionScheduleItem& next);

    bool isFinished();

    void stop(ReplaySchedulerState state, WebCore::EventActionRegister* eventActionRegister) {
        if (m_doneEmitted) {
            return;
        }

        m_state = state;

        if (state == FINISHED) {
            // Emit all remaining event actions as potential errors
            // Notice, that we do not record the remaining event actions in the original
            // exection. Thus, this will be an over approximation of the actual list of
            // new event actions.

            std::set<std::string> names = eventActionRegister->getWaitingNames();
            std::set<std::string>::const_iterator iter;

            for (iter = names.begin(); iter != names.end(); iter++) {
                WTF::WarningCollectorReport("WEBERA_SCHEDULER", "Non-executed event action.", (*iter));
            }
        }

        stop();
    }

    void stop(ReplaySchedulerState state) {
        if (m_doneEmitted) {
            return;
        }

        m_state = state;

        stop();
    }

    void stop() {
        if (m_doneEmitted) {
            return;
        }

        m_doneEmitted = true;

        m_networkProvider->setMode(STOP);
        m_timeProvider->setMode(STOP);
        m_randomProvider->setMode(STOP);
        m_mode = STOP;

        emit sigDone();
    }

    ReplaySchedulerState getState() {
        return m_state;
    }

    void timeout();

private:

    bool executeDelayedEventAction(WebCore::EventActionRegister* eventActionRegister);

    void debugPrintTimers(std::ostream& out, WebCore::EventActionRegister* eventActionRegister);

    WebCore::EventActionSchedule* m_schedule;
    WTF::Vector<WebCore::EventActionScheduleItem> m_schedule_backlog;

    QNetworkReplyControllableFactoryReplay* m_networkProvider;
    TimeProviderReplay* m_timeProvider;
    RandomProviderReplay* m_randomProvider;

    ReplayMode m_mode;
    ReplaySchedulerState m_state;
    bool m_doneEmitted;

    QTimer m_eventActionTimeoutTimer;
    bool m_skipAfterNextTry;

    bool m_timeout_use_aggressive;
    unsigned int m_timeout_miliseconds;
    unsigned int m_timeout_aggressive_miliseconds;

    WTF::EventActionId m_nextEventActionId;

private slots:
    void slEventActionTimeout();

signals:
    void sigDone();
};

#endif // REPLAYSCHEDULER_H
