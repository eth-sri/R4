/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
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

#include <QObject>
#include <QHash>
#include <QList>

#include <config.h>

#include <WebCore/platform/ThreadTimers.h>
#include <WebCore/platform/ThreadGlobalData.h>

#include "clientapplication.h"
#include "replayscheduler.h"
#include "network.h"
#include "datalog.h"

class ReplayClientApplication : public ClientApplication {
    Q_OBJECT

public:
    ReplayClientApplication(int& argc, char** argv);

private:
    void handleUserOptions();

    QString m_url;
    QString m_schedulePath;
    TimeProviderReplay* m_timeProvider;

    bool m_isStopping;

public slots:
    void slSchedulerDone();
};

ReplayClientApplication::ReplayClientApplication(int& argc, char** argv)
    : ClientApplication(argc, argv)
    , m_timeProvider(new TimeProviderReplay("/tmp/log.data"))
    , m_isStopping(false)
{
    handleUserOptions();

    m_timeProvider->attach();

    ReplayScheduler* scheduler = new ReplayScheduler(m_schedulePath.toStdString(), m_timeProvider);
    QObject::connect(scheduler, SIGNAL(sigDone()), this, SLOT(slSchedulerDone()));

    WebCore::ThreadTimers::setScheduler(scheduler);
    WebCore::QNetworkReplyControllableFactory::setFactory(new QNetworkReplyControllableFactoryReplay());

    m_window->page()->currentFrame()->enableReplayUserEventMode();

    loadWebsite(m_url);
}

void ReplayClientApplication::handleUserOptions()
{
    QStringList args = arguments();

    if (args.contains(QString::fromAscii("-help")) || args.size() == 1) {
        qDebug() << "Usage:" << m_programName.toLatin1().data() << "<URL> <schedule>";
        exit(0);
    }

    m_url = args.at(1);
    m_schedulePath = args.at(2);
}

void ReplayClientApplication::slSchedulerDone()
{
    if (m_isStopping == false) {

        uint htmlHash = 0; // this will overflow as we are using it, but that is as exptected

        QList<QWebFrame*> queue;
        queue.append(m_window->page()->mainFrame());

        while (!queue.empty()) {
            QWebFrame* current = queue.takeFirst();
            htmlHash += qHash(current->toHtml());
            queue.append(current->childFrames());
        }

        std::cout << "Schedule executed successfully" << std::endl;
        std::cout << "HTML-hash: " << htmlHash << std::endl; // TODO the hash should be constructed for each frame

        m_window->close();
        m_isStopping = true;
    }
}


int main(int argc, char **argv)
{
    ReplayClientApplication app(argc, argv);

#ifndef NDEBUG
    int retVal = app.exec();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

#include "main.moc"
