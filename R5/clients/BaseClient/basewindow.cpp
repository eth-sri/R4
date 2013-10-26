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

#include "basewindow.h"

#include "locationedit.h"

#include <QAction>
#include <QFileInfo>
#include <QDebug>

BaseWindow::BaseWindow()
    : m_page(new QWebPage(this))
    , m_toolBar(0)
    , urlEdit(0)
{
    setAttribute(Qt::WA_DeleteOnClose);

    buildUI();
}

void BaseWindow::buildUI()
{
    delete m_toolBar;

    m_toolBar = addToolBar("Navigation");

#ifndef QT_NO_INPUTDIALOG
    urlEdit = new LocationEdit(m_toolBar);
    urlEdit->setEnabled(false);

    urlEdit->setSizePolicy(QSizePolicy::Expanding, urlEdit->sizePolicy().verticalPolicy());
    m_toolBar->addWidget(urlEdit);

    connect(page()->mainFrame(), SIGNAL(urlChanged(QUrl)), this, SLOT(setAddressUrl(QUrl)));
    connect(page(), SIGNAL(loadProgress(int)), urlEdit, SLOT(setProgress(int)));
#endif

    connect(page()->mainFrame(), SIGNAL(loadStarted()), this, SLOT(onLoadStarted()));
    connect(page()->mainFrame(), SIGNAL(iconChanged()), this, SLOT(onIconChanged()));
    connect(page()->mainFrame(), SIGNAL(titleChanged(QString)), this, SLOT(onTitleChanged(QString)));
    connect(page(), SIGNAL(windowCloseRequested()), this, SLOT(close()));

#ifndef QT_NO_SHORTCUT
#ifndef QT_NO_UNDOSTACK
    page()->action(QWebPage::Undo)->setShortcut(QKeySequence::Undo);
    page()->action(QWebPage::Redo)->setShortcut(QKeySequence::Redo);
#endif
    page()->action(QWebPage::Cut)->setShortcut(QKeySequence::Cut);
    page()->action(QWebPage::Copy)->setShortcut(QKeySequence::Copy);
    page()->action(QWebPage::Paste)->setShortcut(QKeySequence::Paste);
    page()->action(QWebPage::SelectAll)->setShortcut(QKeySequence::SelectAll);
#endif
}

void BaseWindow::setPage(QWebPage* page)
{
    delete m_page;
    m_page = page;

    buildUI();
}

QWebPage* BaseWindow::page() const
{
    return m_page;
}

void BaseWindow::setAddressUrl(const QUrl& url)
{
    setAddressUrl(url.toString(QUrl::RemoveUserInfo));
}

void BaseWindow::setAddressUrl(const QString& url)
{
#ifndef QT_NO_INPUTDIALOG
    if (!url.contains("about:"))
        urlEdit->setText(url);
#endif
}

void BaseWindow::load(const QString& url)
{
    QString input(url);

    QFileInfo fi(input);
    if (fi.exists() && fi.isRelative())
        input = fi.absoluteFilePath();

    QUrl qurl = QUrl::fromUserInput(input);

    if (qurl.scheme().isEmpty())
        qurl = QUrl("http://" + url + "/");
    load(qurl);
}

void BaseWindow::load(const QUrl& url)
{
    if (!url.isValid())
        return;

    setAddressUrl(url.toString());
    page()->mainFrame()->load(url);
}

QString BaseWindow::addressUrl() const
{
#ifndef QT_NO_INPUTDIALOG
    return urlEdit->text();
#endif
    return QString();
}

void BaseWindow::onIconChanged()
{
#ifndef QT_NO_INPUTDIALOG
    urlEdit->setPageIcon(page()->mainFrame()->icon());
#endif
}

void BaseWindow::onLoadStarted()
{
#ifndef QT_NO_INPUTDIALOG
    urlEdit->setPageIcon(QIcon());
#endif
}

void BaseWindow::onTitleChanged(const QString& title)
{
    if (title.isEmpty())
        setWindowTitle(QCoreApplication::applicationName());
    else
        setWindowTitle(QString::fromLatin1("%1 - %2").arg(title).arg(QCoreApplication::applicationName()));
}

void BaseWindow::closeEvent(QCloseEvent *event)
{
    emit sigOnCloseEvent();
    QMainWindow::closeEvent(event);
}
