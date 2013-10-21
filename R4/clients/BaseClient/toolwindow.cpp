/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Kristof Kosztyo <Kosztyo.Kristof@stud.u-szeged.hu>
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

#include "toolwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#ifndef QT_NO_SHORTCUT
#include <QMenuBar>
#endif
#include <QSlider>
#include <QSplitter>
#include <QVBoxLayout>
#if !defined(QT_NO_FILEDIALOG) && !defined(QT_NO_MESSAGEBOX)
#include <QFileDialog>
#endif

const int gExitClickArea = 80;

ToolWindow::ToolWindow()
    : BaseWindow()
    , m_view(0)
    , m_inspector(0)
{
    init();
    createChrome();
}

ToolWindow::~ToolWindow()
{
}

void ToolWindow::init()
{
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    setCentralWidget(splitter);

    resize(800, 600);

    m_inspector = new QWebInspector;
    connect(this, SIGNAL(destroyed()), m_inspector, SLOT(deleteLater()));

    initializeView();
}

void ToolWindow::initializeView()
{
    delete m_view;

    m_inputUrl = addressUrl();
    QUrl url = page()->mainFrame()->url();
    setPage(new QWebPage(this));

    QSplitter* splitter = static_cast<QSplitter*>(centralWidget());

    QWebView* view = new QWebView(splitter);
    view->setPage(page());

    m_view = view;

    connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));

    applyPrefs();

    splitter->addWidget(m_inspector);
    m_inspector->setPage(page());
    m_inspector->hide();

    if (url.isValid())
        page()->mainFrame()->load(url);
    else  {
        setAddressUrl(m_inputUrl);
        m_inputUrl = QString();
    }
}

void ToolWindow::setRemoteInspectionPort(uint port)
{
    page()->setProperty("_q_webInspectorServerPort", port);
}

void ToolWindow::applyPrefs()
{
    QWebSettings* settings = page()->settings();
    settings->setAttribute(QWebSettings::AcceleratedCompositingEnabled, false);
    settings->setAttribute(QWebSettings::TiledBackingStoreEnabled, false);
    settings->setAttribute(QWebSettings::FrameFlatteningEnabled, false);
    settings->setAttribute(QWebSettings::WebGLEnabled, false);
}

void ToolWindow::createChrome()
{
#ifndef QT_NO_SHORTCUT
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    toolsMenu->addAction("Take Screen Shot...", this, SLOT(screenshot()));
    toolsMenu->addAction("Show Web Inspector", m_inspector, SLOT(setVisible(bool)), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));
#endif
}

void ToolWindow::loadStarted()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

void ToolWindow::loadFinished()
{
    QUrl url = page()->mainFrame()->url();
    if (m_inputUrl.isEmpty())
        setAddressUrl(url.toString(QUrl::RemoveUserInfo));
    else {
        setAddressUrl(m_inputUrl);
        m_inputUrl = QString();
    }
}

void ToolWindow::screenshot()
{
    QPixmap pixmap = QPixmap::grabWidget(m_view);
    QLabel* label = 0;
    label = new QLabel;
    label->setAttribute(Qt::WA_DeleteOnClose);
    label->setWindowTitle("Screenshot - Preview");
    label->setPixmap(pixmap);
    label->show();

#ifndef QT_NO_FILEDIALOG
    QString fileName = QFileDialog::getSaveFileName(label, "Screenshot");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName, "png");
        if (label)
            label->setWindowTitle(QString("Screenshot - Saved at %1").arg(fileName));
    }
#endif
}

ToolWindow* ToolWindow::newWindow()
{
    ToolWindow* mw = new ToolWindow();
    mw->show();
    return mw;
}

