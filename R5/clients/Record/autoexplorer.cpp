/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 University of Szeged
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

#include "autoexplorer.h"

#include <iostream>
#include <QDebug>
#include <QWebPage>

AutoExplorer::AutoExplorer(QMainWindow* window, QWebFrame* frame)
    : m_window(window)
    , m_frame(frame)
    , m_numFramesLoading(0)
{

    // Track the current number of frames being loaded
    // loadStarted and loadFinished on QWebPage is emitted for each frame/sub-frame

    connect(m_frame->page(), SIGNAL(loadStarted()), this, SLOT(frameLoadStarted()));
    connect(m_frame->page(), SIGNAL(loadFinished(bool)), this, SLOT(frameLoadFinished()));

    // Monitor page changes

    connect(m_frame, SIGNAL(pageChanged()), this, SLOT(differentPage()));

    // Stop when auto exploration is done

    connect(m_frame, SIGNAL(automaticExplorationDone()), this, SLOT(stop()));

    // This timer is used to reactivate auto exploration, since auto exploration stops
    // if there are nothing to do (often happens when initializing a page)

    m_explorationKeepAliveTimer.setInterval(10);
    connect(&m_explorationKeepAliveTimer, SIGNAL(timeout()), this, SLOT(explorationKeepAlive()));

}

void AutoExplorer::explore(const QString& url, unsigned int preExploreTimeout, unsigned int explorationTimeout)
{
    m_frame->load(url);

    // preExploreTimeout - the time we will allow the analysis to run before the page is loaded and exploration starts
    // if we can't reach this timeout then we will stop

    if (preExploreTimeout > 0) {
        m_preExplorationTimer.setInterval(preExploreTimeout * 1000);
        m_preExplorationTimer.setSingleShot(true);

        connect(m_frame, SIGNAL(loadStarted()), &m_preExplorationTimer, SLOT(start()));
        connect(&m_preExplorationTimer, SIGNAL(timeout()), this, SLOT(stop()));

        m_preExplorationTimer.start();
    }

    // m_explorationTimer - limit the timeframe in which we do exploration

    if (explorationTimeout > 0) {
        m_explorationTimer.setInterval(explorationTimeout * 1000);
        m_explorationTimer.setSingleShot(true);
        connect(&m_explorationTimer, SIGNAL(timeout()), this, SLOT(stop()));
    }
}

void AutoExplorer::explorationKeepAlive()
{
    if (m_numFramesLoading == 0) {
        m_frame->runAutomaticExploration();
    }

    m_explorationKeepAliveTimer.start();
}

void AutoExplorer::stop() {
    if (!m_frame->isAutomaticExplorationDone()) {
        std::cerr << "Warning: Auto exploration not finished before stopping." << std::endl;
    }

    disconnect(m_frame, 0, this, 0);
    emit done();
}

void AutoExplorer::frameLoadStarted()
{
    ++m_numFramesLoading;
}

void AutoExplorer::frameLoadFinished()
{
    if (m_numFramesLoading > 0) {
        --m_numFramesLoading;
    }

    if (m_numFramesLoading == 0 && !m_explorationTimer.isActive()) {
        // only do this once

        m_explorationKeepAliveTimer.start();
        m_explorationTimer.start();

        m_preExplorationTimer.stop();
    }
}

void AutoExplorer::differentPage()
{
    fprintf(stderr, "Page changed. Stopping automation.\n");
    stop();
}
