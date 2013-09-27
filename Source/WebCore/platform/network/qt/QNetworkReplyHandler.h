/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef QNetworkReplyHandler_h
#define QNetworkReplyHandler_h

#include <QObject>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "FormData.h"
#include "QtMIMETypeSniffer.h"
#include "WebCore/platform/Timer.h"

#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include "ResourceError.h"

QT_BEGIN_NAMESPACE
class QFile;
class QNetworkReply;
QT_END_NAMESPACE

namespace WebCore {

class FormDataIODevice;
class ResourceError;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
class QNetworkReplyHandler;

class QNetworkReplyHandlerCallQueue : public QObject {
    Q_OBJECT
public:
    QNetworkReplyHandlerCallQueue(QNetworkReplyHandler*, bool deferSignals);
    bool deferSignals() const { return m_deferSignals; }
    void setDeferSignals(bool, bool sync = false);

    typedef void (QNetworkReplyHandler::*EnqueuedCall)();
    void push(EnqueuedCall method);
    void clear() { m_enqueuedCalls.clear(); }

    void lock();
    void unlock();
private:
    QNetworkReplyHandler* m_replyHandler;
    int m_locks;
    bool m_deferSignals;
    bool m_flushing;
    QList<EnqueuedCall> m_enqueuedCalls;

    Q_INVOKABLE void flush();
};

class QNetworkReplyWrapper : public QObject {
    Q_OBJECT
public:
    QNetworkReplyWrapper(QNetworkReplyHandlerCallQueue*, QNetworkReply*, bool sniffMIMETypes, QObject* parent = 0);
    ~QNetworkReplyWrapper();

    QNetworkReply* reply() const { return m_reply; }
    QNetworkReply* release();

    void synchronousLoad();

    QUrl redirectionTargetUrl() const { return m_redirectionTargetUrl; }
    QString encoding() const { return m_encoding; }
    QString advertisedMIMEType() const { return m_advertisedMIMEType; }
    QString mimeType() const { return m_sniffedMIMEType.isEmpty() ? m_advertisedMIMEType : m_sniffedMIMEType; }

    bool responseContainsData() const { return m_responseContainsData; }
    bool wasRedirected() const { return m_redirectionTargetUrl.isValid(); }

    // See setFinished().
    bool isFinished() const { return m_reply->property("_q_isFinished").toBool(); }

private Q_SLOTS:
    void receiveMetaData();
    void didReceiveFinished();
    void didReceiveReadyRead();
    void receiveSniffedMIMEType();
    void setFinished();

private:
    void resetConnections();
    void emitMetaDataChanged();

    QNetworkReply* m_reply;
    QUrl m_redirectionTargetUrl;

    QString m_encoding;
    QNetworkReplyHandlerCallQueue* m_queue;
    bool m_responseContainsData;

    QString m_advertisedMIMEType;

    QString m_sniffedMIMEType;
    OwnPtr<QtMIMETypeSniffer> m_sniffer;
    bool m_sniffMIMETypes;
};

/**
 * WebERA:
 *
 * Change QNetworkReplyHandler such that we can control the loading of resources in a detailed manner.
 *
 * Roughly, incoming network data is processed as follows:
 * Qt network handing -> QNetworkReplyHandlerCallQueue -> QNetworkReplyHandler -> client
 *
 * We insert a delay between QNetworkReplyHandler and its client by registering timers in ThreadTimers. Each timer
 * is given a relevant event action descriptor representing the event it is delaying.
 *
 * Further processing of network events are stopped in QNetworkReplyHandlerCallQueue until the current timer has fired.
 *
 * The following invariants are assumed:
 * (1) The URL of the requested resource is unique*
 * (2) The resulting sequence of timers are triggered in the same order as they are registered**
 * (3) The response is divided into identical data chunks from execution to execution***
 *   (This assumes that (3a) the response is identical from execution to execution and (3b) the underlying infrastructure divides this into equal chunks
 *
 * The following events are registered:
 *
 * QNetworkReplyHandler(FAILURE, <url>)
 * QNetworkReplyHandler(FINISHED, <url>)
 * QNetworkReplyHandler(DATA, <chunk-seq-number>, <url>)
 * QNetworkReplyHandler(UPLOAD, <chunk-seq-number>, <url>)
 * QNetworkReplyHandler(RESPONSE, <url>)
 * QNetworkReplyHandler(REQUEST, <url>)
 *
 * * If we want to support multiple requets to the same URL then we need to improve the Timer names.
 * ** This holds as we do not expose the next timer before the previous timer has been fired
 * *** TODO(WebERA) check if this invariant holds - right now this does not hold. When we use the CallQueue to defer further processing new data is still received and added,
 *     thus depending on when we process the queue the chunks extracted will be different.
 *
 */

class QNetworkReplyHandler : public QObject
{
    Q_OBJECT
public:
    enum LoadType {
        AsynchronousLoad,
        SynchronousLoad
    };

    QNetworkReplyHandler(ResourceHandle*, LoadType, bool deferred = false);
    void setLoadingDeferred(bool deferred) { m_queue.setDeferSignals(deferred, m_loadType == SynchronousLoad); }

    QNetworkReply* reply() const { return m_replyWrapper ? m_replyWrapper->reply() : 0; }

    void abort();

    QNetworkReply* release();

    void finish();
    void forwardData();
    void sendResponseIfNeeded();

    // WebERA delayed callbacks
    void delayedFinish(Timer<QNetworkReplyHandler>* timer);
    void delayedFailure(Timer<QNetworkReplyHandler>* timer);
    void delayedResponse(Timer<QNetworkReplyHandler>* timer);
    void delayedRequest(Timer<QNetworkReplyHandler>* timer);
    void delayedDataSent(Timer<QNetworkReplyHandler>* timer);
    void delayedDataReceived(Timer<QNetworkReplyHandler>* timer);

    static ResourceError errorForReply(QNetworkReply*);

private slots:
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);

private:
    void start();
    String httpMethod() const;
    void redirect(ResourceResponse&, const QUrl&);
    bool wasAborted() const { return !m_resourceHandle; }
    QNetworkReply* sendNetworkRequest(QNetworkAccessManager*, const ResourceRequest&);
    FormDataIODevice* getIODevice(const ResourceRequest&);
    void clearContentHeaders();

    OwnPtr<QNetworkReplyWrapper> m_replyWrapper;
    ResourceHandle* m_resourceHandle;
    LoadType m_loadType;
    QNetworkAccessManager::Operation m_method;
    QNetworkRequest m_request;

    // defer state holding
    int m_redirectionTries;

    QNetworkReplyHandlerCallQueue m_queue;

    Timer<QNetworkReplyHandler> m_finishedDelayTimer;
    Timer<QNetworkReplyHandler> m_failureDelayTimer;
    Timer<QNetworkReplyHandler> m_responseDelayTimer;
    Timer<QNetworkReplyHandler> m_requestDelayTimer;
    Timer<QNetworkReplyHandler> m_dataSentDelayTimer;
    Timer<QNetworkReplyHandler> m_dataReceivedDelayTimer;

    ResourceResponse m_deferredResponse;
    ResourceRequest m_deferredRequest;
    ResourceError m_deferredError;
    qint64 m_deferredBytesSent;
    qint64 m_deferredBytesTotal;
    QByteArray m_deferredBytes;
};

// Self destructing QIODevice for FormData
//  For QNetworkAccessManager::put we will have to gurantee that the
//  QIODevice is valid as long finished() of the QNetworkReply has not
//  been emitted. With the presence of QNetworkReplyHandler::release I do
//  not want to gurantee this.
class FormDataIODevice : public QIODevice {
    Q_OBJECT
public:
    FormDataIODevice(FormData*);
    ~FormDataIODevice();

    bool isSequential() const;
    qint64 getFormDataSize() const { return m_fileSize + m_dataSize; }

protected:
    qint64 readData(char*, qint64);
    qint64 writeData(const char*, qint64);

private:
    void moveToNextElement();
    qint64 computeSize();
    void openFileForCurrentElement();

private:
    Vector<FormDataElement> m_formElements;
    QFile* m_currentFile;
    qint64 m_currentDelta;
    qint64 m_fileSize;
    qint64 m_dataSize;
};

}

#endif // QNetworkReplyHandler_h
