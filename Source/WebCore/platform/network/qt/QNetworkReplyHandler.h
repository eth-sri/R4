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

#include "WebCore/platform/Timer.h"

#include "FormData.h"
#include "QtMIMETypeSniffer.h"

#include <QNetworkReply>

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

namespace WebCore {

class FormDataIODevice;
class ResourceError;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
class QNetworkReplyHandler;
class QNetworkReplyWrapper;

/**
 * WebERA:
 *
 * Change QNetworkReplyWrapper such that we can control the loading of resources in a detailed manner.
 *
 * All initial calls are made to the wrapper, which then queues sequences of finegrained events in the
 * QNetworkReplyHandlerCallQueue. Each call to QNetworkReplyWrapper represents one atomic operation as seen
 * from the network, and this is then further split into small events in JavaScript.
 *
 * Here we control the execution of these atomic blocks. We intercept each call to QNetworkReplyWrapper and
 * saves that call in a queue including a complete snapshot of the state of QNetworkReply.
 *
 * This allows us to:
 * 1. Control when these calls are processed without worring that calls queued later are interferring with the state
 * 2. Save the snapshots in order to precisely replay the same sequence of atomic events
 *
 * A timer is registered as a callback to process the next atomic block in the queue.
 *
 * Right now this only allows for repeating the same sequence of events if it occurs again. It does not support
 * replaying events reusing these snapshots.
 *
 * The following invariants are assumed:
 * (1) The URL of the requested resource is unique*
 * (2) The resulting sequence of timers are triggered in the same order as they are registered**
 * (3) The response is divided into identical data chunks from execution to execution***
 *   (This assumes that (3a) the response is identical from execution to execution and (3b) the underlying infrastructure divides this into equal chunks
 *
 * The following events are registered:
 *
 * NETWORK(<url>, <url-seq-number>, <seq-number>)
 *
 * Happens before relations are added between each of these atomic blocks.
 *
 * TODO(WebERA): Should upload events be allowed to be triggered out of sequence?
 * TODO(WebERA): Add the snapshotting functionality to the snapshots
 *
 */

class QNetworkReplySnapshot {

public:

    QNetworkReplySnapshot(QNetworkReply* reply);

    QVariant header(QNetworkRequest::KnownHeaders header) const { return m_reply->header(header); }
    bool hasRawHeader(const QByteArray &headerName) const { return m_reply->hasRawHeader(headerName); }
    QList<QByteArray> rawHeaderList() const { return m_reply->rawHeaderList(); }
    QByteArray rawHeader(const QByteArray &headerName) const { return m_reply->rawHeader(headerName); }
    const QList<QNetworkReply::RawHeaderPair>& rawHeaderPairs() const { return m_reply->rawHeaderPairs(); }

    QVariant attribute(QNetworkRequest::Attribute code) const { return m_reply->attribute(code); }
    QNetworkReply::NetworkError error() const { return m_reply->error(); }
    QUrl url() const { return m_reply->url(); }

    // QObject
    QVariant property(const char * name) const { return m_reply->property(name); }
    bool setProperty(const char *name, const QVariant &value) { return m_reply->setProperty(name, value); }

    // QIODevice
    qint64 bytesAvailable() const { return m_reply->bytesAvailable(); }
    QByteArray read(qint64 maxlen) { return m_reply->read(maxlen); }
    QByteArray peek(qint64 maxlen) { return m_reply->peek(maxlen); }
    QString errorString() const { return m_reply->errorString(); }

private:
    QNetworkReply* m_reply;
};

/**
 * WebERA:
 *
 * Exposes the same API as a QNetworkReply, only that the current state is pulled from QNetworkReplySnapshots.
 *
 * Notice that this class is subclassable, such that alternative wrappers (e.g. a replay wrapper) can be used.
 * Change the QNetworkReplyControllableFactory if you want to instanciate connections using different subclasses.
 *
 * This base class handles scheduling through timers and signalling to clients. Subclasses should use the
 * enqueueSnapshot function to queue relevant snapshots.
 */
class QNetworkReplyControllable : public QObject {
    Q_OBJECT

public:
    QNetworkReplyControllable(QNetworkReply* reply, QObject* parent = 0);
    ~QNetworkReplyControllable();

    QNetworkReplySnapshot* snapshot() { return m_currentSnapshot; }

    // Subclasses should use this as an early dereference
    virtual QNetworkReply* release();

    void updateSnapshot(Timer<QNetworkReplyControllable>* timer);

signals:
    void finished();
    void readyRead();

protected:

    virtual void detachFromReply();

    enum NetworkSignal {
        FINISHED,
        READY_READ
    };

    void enqueueSnapshot(NetworkSignal signal, QNetworkReplySnapshot* snapshot);

    typedef QPair<NetworkSignal, QNetworkReplySnapshot*> QueuedSnapshot;
    QList<QueuedSnapshot> m_snapshotQueue;
    QNetworkReplySnapshot* m_currentSnapshot;


    unsigned long m_sequenceNumber;
    bool m_nextSnapshotUpdateTimerRunning;
    Timer<QNetworkReplyControllable> m_nextSnapshotUpdateTimer;
    EventActionDescriptor m_parentDescriptor;

protected slots:
    void scheduleNextSnapshotUpdate();

protected:
    QNetworkReply* m_reply;

};

/**
 * WebERA: This subclass of QNetworkReplyControllable uses the reply and communicates with the outside world.
 */
class QNetworkReplyControllableLive : public QNetworkReplyControllable {
    Q_OBJECT

public:
    QNetworkReplyControllableLive(QNetworkReply*, QObject* parent = 0);
    ~QNetworkReplyControllableLive();

protected:
    virtual void detachFromReply();

public slots:
    void slFinished();
    void slReadyRead();

};

// WebERA STOP

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

    QNetworkReplyControllable* reply() const { return m_reply; }
    QNetworkReply* release();

    void synchronousLoad();

    QUrl redirectionTargetUrl() const { return m_redirectionTargetUrl; }
    QString encoding() const { return m_encoding; }
    QString advertisedMIMEType() const { return m_advertisedMIMEType; }
    QString mimeType() const { return m_sniffedMIMEType.isEmpty() ? m_advertisedMIMEType : m_sniffedMIMEType; }

    bool responseContainsData() const { return m_responseContainsData; }
    bool wasRedirected() const { return m_redirectionTargetUrl.isValid(); }

    // See setFinished().
    bool isFinished() const { return m_reply->snapshot()->property("_q_isFinished").toBool(); }

private Q_SLOTS:
    void receiveMetaData();
    void didReceiveFinished();
    void didReceiveReadyRead();
    void receiveSniffedMIMEType();
    void setFinished();

private:
    void resetConnections();
    void emitMetaDataChanged();

    QNetworkReplyControllable* m_reply;
    QUrl m_redirectionTargetUrl;

    QString m_encoding;
    QNetworkReplyHandlerCallQueue* m_queue;
    bool m_responseContainsData;

    QString m_advertisedMIMEType;

    QString m_sniffedMIMEType;
    OwnPtr<QtMIMETypeSniffer> m_sniffer;
    bool m_sniffMIMETypes;
};

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

    //QNetworkReply* reply() const { return m_replyWrapper ? m_replyWrapper->reply() : 0; } // WebERA: Lets see if we can avoid exposing this

    void abort();

    QNetworkReply* release();

    void finish();
    void forwardData();
    void sendResponseIfNeeded();

    static ResourceError errorForReply(QNetworkReplyControllable*);

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
