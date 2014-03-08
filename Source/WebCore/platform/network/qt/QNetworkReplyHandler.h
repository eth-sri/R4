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

#include <algorithm>
#include <set>
#include <list>

#include <QObject>
#include <QList>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkCookie>
#include <QNetworkCookieJar>

#include "wtf/ExportMacros.h"
#include "WebCore/platform/Timer.h"

#include "WebCore/platform/network/FormData.h"
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
 * WebERA
 *
 * By default, each QNetworkReply injects cookies as soon as they are available, regardless of when they are processed by
 * WebKit (which is a problem when we have deferred their processing or want to replay them at a later stage).
 *
 * We use this cookie jar to disable all conventional writes to the cookie store from non-schedulable sources.
 */
class QNetworkSnapshotCookieJar : public QNetworkCookieJar {
    Q_OBJECT

public:
    QNetworkSnapshotCookieJar(QObject* parent = 0);

    bool setCookiesFromUrlUnrestricted(const QList<QNetworkCookie>&, const QUrl&);
    virtual bool setCookiesFromUrl(const QList<QNetworkCookie>&, const QUrl&);
    virtual QList<QNetworkCookie> cookiesForUrl(const QUrl&) const;
};


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
 */
class QNetworkReplySnapshot;

class QNetworkReplyInitialSnapshot
{
    friend class QNetworkReplySnapshot;

public:

    enum NetworkSignal {
        INITIAL,
        FINISHED,
        READY_READ,
        END
    };

    QNetworkReplyInitialSnapshot(QNetworkReply* reply);
    ~QNetworkReplyInitialSnapshot();

    QNetworkReplySnapshot* takeSnapshot(NetworkSignal signal, QNetworkReply* reply);

    unsigned int getSameUrlSequenceNumber() {
        return m_sameUrlSequenceNumber;
    }

    const QUrl& getUrl() {
        return m_url;
    }

    QList<QNetworkCookie> getCookies();

    typedef QPair<NetworkSignal, QNetworkReplySnapshot*> QNetworkReplySnapshotEntry;
    QList<QNetworkReplySnapshotEntry>* getSnapshots() {
        return &m_snapshots;
    }

    QList<QNetworkReplySnapshotEntry>* getSnapshotsCopy() {
        return new QList<QNetworkReplySnapshotEntry>(m_snapshots);
    }

    void serialize(QIODevice* stream) const;
    static QNetworkReplyInitialSnapshot* deserialize(QIODevice* stream);

    static unsigned int getNextSameUrlSequenceNumber(const QUrl& url);

protected:

    QNetworkReplyInitialSnapshot();

    qint64 streamPosition() const { return m_streamPosition; }
    qint64 streamSize() const { return m_stream.size(); }

    QByteArray read(qint64 maxlen);
    QByteArray peek(qint64 maxlen);

    QList<QNetworkReply::RawHeaderPair> m_headers;
    QVariant m_cookies;

    unsigned int m_sameUrlSequenceNumber;
    QUrl m_url;

    qint64 m_streamPosition; // points at the next value to read
    QByteArray m_stream;

    QList<QNetworkReplySnapshotEntry> m_snapshots;

private:
    static QHash<QString, unsigned int> m_nextSameUrlSequenceNumber;

};

class QNetworkReplySnapshot
{
    friend class QNetworkReplyInitialSnapshot;

private:

    QNetworkReplySnapshot(QNetworkReply* reply, QNetworkReplyInitialSnapshot* base);
    QNetworkReplySnapshot(QNetworkReplyInitialSnapshot* base);


public:

    // Base values

    QByteArray rawHeader(const QByteArray &headerName) const;
    const QList<QNetworkReply::RawHeaderPair>& rawHeaderPairs() const { return m_base->m_headers; }

    QUrl url() const { return m_base->m_url; }

    // Snapshot state

    QNetworkReply::NetworkError error() const { return m_error; }
    QString errorString() const { return m_errorString; }

    QVariant attribute(QNetworkRequest::Attribute code) const {
        ASSERT(code <= 2); // We only store the 3 first attributes, it seems we are only using these
        return m_attributes.at(code);
    }

    // Snapshot stream

    qint64 bytesAvailable() const {
        return m_streamSize - m_base->streamPosition();
    }

    QByteArray read(qint64 maxlen) {
        return m_base->read(std::min(bytesAvailable(), maxlen));
    }

    QByteArray peek(qint64 maxlen) {
        return m_base->peek(std::min(bytesAvailable(), maxlen));
    }

private:
    QNetworkReplyInitialSnapshot* m_base;

    QNetworkReply::NetworkError m_error;
    QString m_errorString;
    QList<QVariant> m_attributes;

    qint64 m_streamSize;
};

/**
 * WebERA:
 *
 * Wraps QNetworkReply and acts as a proxy between QNetworkReply and the rest of the network code. QNetworkReplyControllable
 * exposes a similar API as QNetworkReply, making the same signals avaialble and all relevant functions for inspecting a reply
 * are exposed through QNetworkReplay snapshots.
 *
 * The purpose of this wrapper is to monitor and control when and how the state of QNetworkReply changes. QNetworkReplyControllable
 * exposes a generic interface in which snapshots of the QNetworkReply is presented, and relevant signals are emitted to the network
 * code when the current snapshot is updated. Timers are used to control (by the scheduler) when the snapshot updates.
 *
 * The default behaviour (implemented in QNetworkReplyControllableLive) is to create these snapshots as the real QNetworkReply emits
 * updates, and queue them for consumption by the network code.
 *
 * This architecture allows for subclasses to implement alternative behaviour, such as fetching snapshots previously observed snapshots
 * (replaying). Change the QNetworkReplyControllableFactory to inject alternative subclasses.
 *
 */

class QNetworkReplyControllableFactory;

class QNetworkReplyControllable : public QObject {
    Q_OBJECT

public:
    QNetworkReplyControllable(QNetworkReplyControllableFactory* factory, QNetworkReply* reply, QObject* parent = 0);
    ~QNetworkReplyControllable();

    QNetworkReplySnapshot* snapshot() { return m_currentSnapshot; }
    QNetworkReplyInitialSnapshot* initialSnapshot() { return m_initialSnapshot; }

    // Subclasses should use this as an early dereference
    virtual QNetworkReply* release();

    void updateSnapshot(Timer<QNetworkReplyControllable>* timer);

signals:
    void finished();
    void readyRead();

protected:

    virtual void detachFromReply();

    void enqueueSnapshot(QNetworkReplyInitialSnapshot::NetworkSignal signal, QNetworkReplySnapshot* snapshot);

    typedef QPair<QNetworkReplyInitialSnapshot::NetworkSignal, QNetworkReplySnapshot*> QueuedSnapshot;
    QList<QueuedSnapshot> m_snapshotQueue;

    QNetworkReplyInitialSnapshot* m_initialSnapshot;
    QNetworkReplySnapshot* m_currentSnapshot;


    unsigned long m_sequenceNumber;
    bool m_nextSnapshotUpdateTimerRunning;
    Timer<QNetworkReplyControllable> m_nextSnapshotUpdateTimer;

    QNetworkReply* m_reply;

    WTF::EventActionId m_lastNetworkEventAction;

    QNetworkReplyControllableFactory* m_factory;

protected slots:
    void scheduleNextSnapshotUpdate();

};

/**
 * WebERA: This subclass of QNetworkReplyControllable uses the reply and communicates with the outside world.
 */
class QNetworkReplyControllableLive : public QNetworkReplyControllable {
    Q_OBJECT

public:
    QNetworkReplyControllableLive(QNetworkReplyControllableFactory* factory, QNetworkReply*, QObject* parent = 0);
    virtual ~QNetworkReplyControllableLive();

protected:
    virtual void detachFromReply();

public slots:
    void slFinished();
    void slReadyRead();

};

/** Controllable Factory **/

class QNetworkReplyControllableFactory
{

public:
    virtual QNetworkReplyControllable* construct(QNetworkReply* reply, QObject* parent=0) = 0;

    QNetworkReplyControllableFactory();
    virtual ~QNetworkReplyControllableFactory();

    void controllableDone(QNetworkReplyControllable* controllable);
    void controllableConstructed(QNetworkReplyControllable* controllable);
    void writeNetworkFile(QString networkFilePath);

    unsigned int doneCounter() const {
        return m_doneCounter;
    }

    static QNetworkReplyControllableFactory* getFactory();
    static void setFactory(QNetworkReplyControllableFactory* factory);

protected:
    std::set<QNetworkReplyControllable*> m_openNetworkSessions;

private:
    unsigned int m_doneCounter;
    std::list<WebCore::QNetworkReplyInitialSnapshot*> m_networkHistory;

    static QNetworkReplyControllableFactory* m_factory;
};

class QNetworkReplyControllableFactoryLive : public QNetworkReplyControllableFactory
{
    ~QNetworkReplyControllableFactoryLive();

public:
    QNetworkReplyControllableLive* construct(QNetworkReply* reply, QObject* parent=0) {
        return new QNetworkReplyControllableLive(this, reply, parent);
    }
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
