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

#include <fstream>
#include <iostream>

#include <QString>
#include <QTimer>
#include <QNetworkProxy>
#include <QNetworkCookie>

#include <WebCore/platform/ThreadTimers.h>
#include <WebCore/platform/ThreadGlobalData.h>
#include <JavaScriptCore/runtime/JSExportMacros.h>
#include <WebCore/platform/network/qt/QNetworkReplyHandler.h>
#include <WebCore/platform/schedule/DefaultScheduler.h>
#include <wtf/warningcollectorreport.h>

#include "utils.h"
#include "clientapplication.h"
#include "specificationscheduler.h"
#include "datalog.h"
#include "autoexplorer.h"

#include "wtf/ActionLogReport.h"

/**
 * () ->
 *  schedule.data log.network.data log.random.data log.time.data ER_actionlog errors.log record.png
 */
class RecordClientApplication : public ClientApplication {
    Q_OBJECT

public:
    RecordClientApplication(int& argc, char** argv);

private:
    void handleUserOptions();

private:
    void snapshotState(QString id);

    bool m_running;

    QString m_url;
    QString m_outdir;

    unsigned int m_autoExplorePreTimout;
    unsigned int m_autoExploreTimout;
    bool m_autoExplore;

    bool m_showWindow;

    WebCore::QNetworkReplyControllableFactoryLive* m_network;
    TimeProviderRecord* m_timeProvider;
    RandomProviderRecord* m_randomProvider;
    //SpecificationScheduler* m_scheduler;
    WebCore::DefaultScheduler* m_scheduler;
    AutoExplorer* m_autoExplorer;

    QList<QNetworkCookie> mPresetCookies;

public slots:
    void slOnCloseEvent();
};

RecordClientApplication::RecordClientApplication(int& argc, char** argv)
    : ClientApplication(argc, argv)
    , m_running(true)
    , m_outdir("/tmp/")
    , m_autoExplorePreTimout(30)
    , m_autoExploreTimout(30)
    , m_autoExplore(false)
    , m_showWindow(true)
    , m_timeProvider(new TimeProviderRecord())
    , m_randomProvider(new RandomProviderRecord())
    //, m_scheduler(new SpecificationScheduler(m_network))
    , m_scheduler(new WebCore::DefaultScheduler())
    , m_autoExplorer(new AutoExplorer(m_window, m_window->page()->mainFrame()))
{
    QObject::connect(m_window, SIGNAL(sigOnCloseEvent()), this, SLOT(slOnCloseEvent()));
    handleUserOptions();

    // Network

    m_network = new WebCore::QNetworkReplyControllableFactoryLive();
    WebCore::QNetworkReplyControllableFactory::setFactory(m_network);

    // Random

    m_randomProvider->attach();

    // Time

    m_timeProvider->attach();

    // Scheduler

    WebCore::ThreadTimers::setScheduler(m_scheduler);

    // Cookies support

    WebCore::QNetworkSnapshotCookieJar* cookieJar = new WebCore::QNetworkSnapshotCookieJar(this);
    m_window->page()->networkAccessManager()->setCookieJar(cookieJar);

    QUrl qurl = m_url;
    QList<QNetworkCookie> bakedCookies;
    foreach (QNetworkCookie cookie, mPresetCookies) {
        cookie.setDomain(qurl.host());
        bakedCookies.append(cookie);
    }

    cookieJar->setCookiesFromUrlUnrestricted(bakedCookies, qurl);

    // Load and explore the website

    if (m_autoExplore) {
        m_autoExplorer->explore(m_url, m_autoExplorePreTimout, m_autoExploreTimout);
        QObject::connect(m_autoExplorer, SIGNAL(done()), this, SLOT(slOnCloseEvent()));
    } else {
        loadWebsite(m_url);
    }

    // Show window while running

    if (m_showWindow) {
        m_window->show();
    }
}

void RecordClientApplication::handleUserOptions()
{
    QStringList args = arguments();

    if (args.contains("-help")) {
        qDebug() << "Usage:" << m_programName.toLatin1().data()
                 << "[-schedule-path]"
                 << "[-autoexplore]"
                 << "[-autoexplore-timeout]"
                 << "[-pre-autoexplore-timeout]"
                 << "[-hidewindow]"
                 << "[-verbose]"
                 << "[-proxy URL:PORT]"
                 << "[-cookie KEY=VALUE]"
                 << "[-ignore-mouse-move]"
                 << "[-out_dir]"
                 << "URL";
        std::exit(0);
    }

    WebCore::threadGlobalData().threadTimers().eventActionRegister()->setVerbose(false);
    int verboseIndex = args.indexOf("-verbose");
    if (verboseIndex != -1) {
        WebCore::threadGlobalData().threadTimers().eventActionRegister()->setVerbose(true);
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

    int ignoreMouseMoveIndex = args.indexOf("-ignore-mouse-move");
    if (ignoreMouseMoveIndex != -1) {
        this->m_window->page()->ignoreMouseMove(true);
    }

    int outdirIndex = args.indexOf("-out_dir");
    if (outdirIndex != -1) {
         m_outdir = takeOptionValue(&args, outdirIndex);
    }

    int timeoutIndex = args.indexOf("-autoexplore-timeout");
    if (timeoutIndex != -1) {
        m_autoExploreTimout = takeOptionValue(&args, timeoutIndex).toInt();
    }

    int preTimeoutIndex = args.indexOf("-pre-autoexplore-timeout");
    if (preTimeoutIndex != -1) {
        m_autoExplorePreTimout = takeOptionValue(&args, preTimeoutIndex).toInt();
    }

    int autoexploreIndex = args.indexOf("-autoexplore");
    if (autoexploreIndex != -1) {
        m_autoExplore = true;
    }

    int windowIndex = args.indexOf("-hidewindow");
    if (windowIndex != -1) {
        m_showWindow = false;
    }

    int cookieIndex = 0;
    while ((cookieIndex = args.indexOf("-cookie", cookieIndex)) != -1) {
        QString cookieRaw = takeOptionValue(&args, cookieIndex);
        QStringList parts = cookieRaw.split(QString("="));

        QNetworkCookie cookie(parts.at(0).toAscii(), parts.at(1).toAscii());
        cookie.setPath(QString("/"));
        mPresetCookies.append(cookie);

        cookieIndex++; // force the next iteration to exclude this index
    }

    // URLS

    int lastArg = args.lastIndexOf(QRegExp("^-.*"));
    QStringList urls = (lastArg != -1) ? args.mid(++lastArg) : args.mid(1);

    if (urls.length() == 0) {
        qDebug() << "URL required";
        std::exit(1);
    }

    m_url = urls.at(0);
}

void RecordClientApplication::snapshotState(QString id) {

    // Set paths

    QString outStatusPath = m_outdir + "/" + id + "status.data";
    QString outSchedulePath = m_outdir + "/" + id + "schedule.data";
    QString outLogNetworkPath = m_outdir + "/" + id + "log.network.data";
    QString outLogTimePath = m_outdir + "/" + id + "log.time.data";
    QString outLogRandomPath = m_outdir + "/" + id + "log.random.data";
    QString outErLogPath = m_outdir + "/" + id + "ER_actionlog";
    QString logErrorsPath = m_outdir + "/" + id + "errors.log";
    QString screenshotPath = m_outdir + "/" + id + "screenshot.png";

    // HTML Hash & scheduler state

    std::ofstream statusfile;
    statusfile.open(outStatusPath.toStdString().c_str());

//    switch (m_scheduler->getState()) {
//    case FINISHED:
//        statusfile << "Result: FINISHED" << std::endl;
//        break;

//    case TIMEOUT:
//        statusfile << "Result: TIMEOUT" << std::endl;
//        break;

//    case ERROR:
//        statusfile << "Result: ERROR" << std::endl;
//        break;

//    default:
//        statusfile << "Result: ERROR" << std::endl;
//        break;
//    }

    uint htmlHash = 0; // this will overflow as we are using it, but that is as exptected

    QList<QWebFrame*> queue;
    queue.append(m_window->page()->mainFrame());

    while (!queue.empty()) {
        QWebFrame* current = queue.takeFirst();
        htmlHash += qHash(current->toHtml());
        queue.append(current->childFrames());
    }

    statusfile << "HTML-hash: " << htmlHash << std::endl;

    statusfile.close();

    // happens before

    ActionLogSave(outErLogPath.toStdString());

    // schedule

    std::ofstream schedulefile;
    schedulefile.open(outSchedulePath.toStdString().c_str());
    WebCore::threadGlobalData().threadTimers().eventActionRegister()->dispatchHistory()->serialize(schedulefile);
    schedulefile.close();

    // network

    m_network->writeNetworkFile(outLogNetworkPath);

    // log

    m_timeProvider->writeLogFile(outLogTimePath);
    m_randomProvider->writeLogFile(outLogRandomPath);

    // Screenshot

    m_window->takeScreenshot(screenshotPath);

    // Errors
    WTF::WarningCollecterWriteToLogFile(logErrorsPath.toStdString());


}

void RecordClientApplication::slOnCloseEvent()
{
    if (!m_running) {
        return; // dont close twice
    }

    m_running = false;

    snapshotState("");

    ActionLogStrictMode(false);

    // Write human readable HB relation dump (DEBUG)
    std::string arcsLogPath = m_outdir.toStdString() + "/arcs.log";
    std::vector<ActionLog::Arc> arcs = ActionLogReportArcs();
    std::ofstream arcslog;
    arcslog.open(arcsLogPath.c_str());

    for (std::vector<ActionLog::Arc>::iterator it = arcs.begin(); it != arcs.end(); ++it) {
        arcslog << (*it).m_tail << " -> " << (*it).m_head << std::endl;
    }

    arcslog.close();

    m_scheduler->stop();
    m_window->close();

    std::cout << "Recording finished" << std::endl;
}


int main(int argc, char **argv)
{
    RecordClientApplication app(argc, argv);

#ifndef NDEBUG
    int retVal = app.exec();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

#include "main.moc"
