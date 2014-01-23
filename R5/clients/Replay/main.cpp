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
#include <QTimer>
#include <QNetworkProxy>

#include <config.h>

#include <WebCore/platform/ThreadTimers.h>
#include <WebCore/platform/ThreadGlobalData.h>
#include <JavaScriptCore/runtime/JSExportMacros.h>
#include <WebCore/platform/network/qt/QNetworkReplyHandler.h>

#include "utils.h"
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
    QString m_logNetworkPath;
    QString m_logRandomPath;
    QString m_logTimePath;

    TimeProviderReplay* m_timeProvider;
    RandomProviderReplay* m_randomProvider;
    QNetworkReplyControllableFactoryReplay* m_network;

    bool m_isStopping;
    bool m_showWindow;

public slots:
    void slSchedulerDone();
    void slEnteredRelaxedMode();
    void slTimeout();
};

ReplayClientApplication::ReplayClientApplication(int& argc, char** argv)
    : ClientApplication(argc, argv)
    , m_isStopping(false)
    , m_showWindow(true)
{

    handleUserOptions();

    // Network

    m_network = new QNetworkReplyControllableFactoryReplay(m_logNetworkPath);

    WebCore::QNetworkReplyControllableFactory::setFactory(m_network);
    m_window->page()->networkAccessManager()->setCookieJar(new WebCore::QNetworkSnapshotCookieJar(this));

    // Random

    m_timeProvider = new TimeProviderReplay(m_logTimePath);
    m_timeProvider->attach();

    // Time

    m_randomProvider = new RandomProviderReplay(m_logRandomPath);
    m_randomProvider->attach();

    // Scheduler

    ReplayScheduler* scheduler = new ReplayScheduler(m_schedulePath.toStdString(), m_timeProvider, m_randomProvider);
    QObject::connect(scheduler, SIGNAL(sigDone()), this, SLOT(slSchedulerDone()));
    QObject::connect(scheduler, SIGNAL(sigEnteredRelaxedReplayMode()), this, SLOT(slEnteredRelaxedMode()));

    WebCore::ThreadTimers::setScheduler(scheduler);

    // Replay-mode setup

    m_window->page()->enableReplayUserEventMode();
    m_window->page()->mainFrame()->enableReplayUserEventMode();

    // Load website and run

    loadWebsite(m_url);

    if (m_showWindow) {
        m_window->show();
    }
}

void ReplayClientApplication::slEnteredRelaxedMode()
{
    m_timeProvider->setRelaxedMode(true);
    m_randomProvider->setRelaxedMode(true);
    m_network->setRelaxedMode(true);
}

void ReplayClientApplication::handleUserOptions()
{
    QStringList args = arguments();

    if (args.contains(QString::fromAscii("-help")) || args.size() == 1) {
        qDebug() << "Usage:" << m_programName.toLatin1().data()
                 << "[-hidewindow]"
                 << "[-timeout]"
                 << "[-proxy URL:PORT]"
                 << "<URL> <schedule> <log.network.data> <log.random.data> <log.time.data>";
        exit(0);
    }

    int windowIndex = args.indexOf("-hidewindow");
    if (windowIndex != -1) {
        m_showWindow = false;
    }

    int proxyUrlIndex = args.indexOf("-proxy");
    if (proxyUrlIndex != -1) {

        QString proxyUrl = takeOptionValue(&args, proxyUrlIndex);
        QStringList parts = proxyUrl.split(QString(":"));

        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(parts.at(0));
        if(parts.length() > 1){
            proxy.setPort(parts.at(1).toShort());
        }
        QNetworkProxy::setApplicationProxy(proxy);

    }

    int timeoutIndex = args.indexOf("-timeout");
    if (timeoutIndex != -1) {
        QTimer::singleShot(takeOptionValue(&args, timeoutIndex).toInt() * 1000, this, SLOT(slTimeout()));
    }

    int lastArg = args.lastIndexOf(QRegExp("^-.*"));
    if (lastArg == -1)
        lastArg = 0;

    m_url = args.at(++lastArg);
    m_schedulePath = args.at(++lastArg);
    m_logNetworkPath = args.at(++lastArg);
    m_logRandomPath = args.at(++lastArg);
    m_logTimePath = args.at(++lastArg);
}

void ReplayClientApplication::slTimeout() {
    std::cerr << "Error, replay timeout reached" << std::endl;
    QApplication::exit(1);
}

void ReplayClientApplication::slSchedulerDone()
{
    if (m_isStopping == false) {

        m_network->stop();
        m_timeProvider->stop();
        m_randomProvider->stop();

        uint htmlHash = 0; // this will overflow as we are using it, but that is as exptected

        QList<QWebFrame*> queue;
        queue.append(m_window->page()->mainFrame());

        while (!queue.empty()) {
            QWebFrame* current = queue.takeFirst();
            htmlHash += qHash(current->toHtml());
            queue.append(current->childFrames());
        }

        std::cout << "Schedule executed successfully" << std::endl;
        std::cout << "HTML-hash: " << htmlHash << std::endl;

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
