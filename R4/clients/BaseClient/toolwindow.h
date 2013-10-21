/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef toolwindow_h
#define toolwindow_h

#include <QtNetwork/QNetworkRequest>

#include <QDebug>

#include <cstdio>
#include <qevent.h>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebinspector.h>
#include <qwebsettings.h>
#include <QWebPage>

#include "basewindow.h"

class QLineEdit;

class ToolWindow : public BaseWindow {
    Q_OBJECT

public:
    ToolWindow();
    virtual ~ToolWindow();

    void setRemoteInspectionPort(uint port);

protected slots:
    void loadStarted();
    void loadFinished();

    void screenshot();

public slots:
    ToolWindow* newWindow();

private:
    void init();
    void initializeView();
    void createChrome();
    void applyPrefs();

private:
    QWidget* m_view;
    QWebInspector* m_inspector;

    QString m_inputUrl;

#ifndef QT_NO_LINEEDIT
    QLineEdit* m_lineEdit;
    int m_findFlag;
    static const int s_findNormalFlag = 0;
#endif
};

#endif
