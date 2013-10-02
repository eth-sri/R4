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

#include <WebCore/platform/ThreadTimers.h>

#include "clientapplication.h"
#include "replayscheduler.h"

class ReplayClientApplication : public ClientApplication {
    Q_OBJECT

public:
    ReplayClientApplication(int& argc, char** argv);

private:
    void handleUserOptions();

    QString m_url;
    QString m_schedulePath;
};

ReplayClientApplication::ReplayClientApplication(int& argc, char** argv)
    : ClientApplication(argc, argv)
{
    handleUserOptions();

    WebCore::ThreadTimers::setScheduler(new ReplayScheduler(m_schedulePath.toStdString()));

    loadWebsite(m_url);
}

void ReplayClientApplication::handleUserOptions()
{
    QStringList args = arguments();

    if (args.contains("-help") || args.size() == 1) {
        qDebug() << "Usage:" << m_programName.toLatin1().data() << "<URL> <schedule>";
        exit(0);
    }

    m_url = args.at(1);
    m_schedulePath = args.at(2);
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
