/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.  <info@staikos.net>
    Copyright (C) 2008 Holger Hans Peter Freyther

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

#include <limits>
#include <sstream>

#include "config.h"
#include "QNetworkReplyHandler.h"
#include <QDataStream>

#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkCookie>

#include <wtf/EventActionDescriptor.h>
#include <wtf/ActionLogReport.h>

#include <wtf/text/CString.h>

#include <QDebug>
#include <QCoreApplication>

// In Qt 4.8, the attribute for sending a request synchronously will be made public,
// for now, use this hackish solution for setting the internal attribute.
const QNetworkRequest::Attribute gSynchronousNetworkRequestAttribute = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::HttpPipeliningWasUsedAttribute + 7);

static const int gMaxRedirections = 10;

namespace WebCore {

// Take a deep copy of the FormDataElement
FormDataIODevice::FormDataIODevice(FormData* data)
    : m_formElements(data ? data->elements() : Vector<FormDataElement>())
    , m_currentFile(0)
    , m_currentDelta(0)
    , m_fileSize(0)
    , m_dataSize(0)
{
    setOpenMode(FormDataIODevice::ReadOnly);

    if (!m_formElements.isEmpty() && m_formElements[0].m_type == FormDataElement::encodedFile)
        openFileForCurrentElement();
    computeSize();
}

FormDataIODevice::~FormDataIODevice()
{
    delete m_currentFile;
}

qint64 FormDataIODevice::computeSize() 
{
    for (int i = 0; i < m_formElements.size(); ++i) {
        const FormDataElement& element = m_formElements[i];
        if (element.m_type == FormDataElement::data) 
            m_dataSize += element.m_data.size();
        else {
            QFileInfo fi(element.m_filename);
            m_fileSize += fi.size();
        }
    }
    return m_dataSize + m_fileSize;
}

void FormDataIODevice::moveToNextElement()
{
    if (m_currentFile)
        m_currentFile->close();
    m_currentDelta = 0;

    m_formElements.remove(0);

    if (m_formElements.isEmpty() || m_formElements[0].m_type == FormDataElement::data)
        return;

    openFileForCurrentElement();
}

void FormDataIODevice::openFileForCurrentElement()
{
    if (!m_currentFile)
        m_currentFile = new QFile;

    m_currentFile->setFileName(m_formElements[0].m_filename);
    m_currentFile->open(QFile::ReadOnly);
}

// m_formElements[0] is the current item. If the destination buffer is
// big enough we are going to read from more than one FormDataElement
qint64 FormDataIODevice::readData(char* destination, qint64 size)
{
    if (m_formElements.isEmpty())
        return -1;

    qint64 copied = 0;
    while (copied < size && !m_formElements.isEmpty()) {
        const FormDataElement& element = m_formElements[0];
        const qint64 available = size-copied;

        if (element.m_type == FormDataElement::data) {
            const qint64 toCopy = qMin<qint64>(available, element.m_data.size() - m_currentDelta);
            memcpy(destination+copied, element.m_data.data()+m_currentDelta, toCopy); 
            m_currentDelta += toCopy;
            copied += toCopy;

            if (m_currentDelta == element.m_data.size())
                moveToNextElement();
        } else {
            const QByteArray data = m_currentFile->read(available);
            memcpy(destination+copied, data.constData(), data.size());
            copied += data.size();

            if (m_currentFile->atEnd() || !m_currentFile->isOpen())
                moveToNextElement();
        }
    }

    return copied;
}

qint64 FormDataIODevice::writeData(const char*, qint64)
{
    return -1;
}

bool FormDataIODevice::isSequential() const
{
    return true;
}

/****************** QNetworkReplySnapshot ******************/

QNetworkReplyInitialSnapshot::QNetworkReplyInitialSnapshot(QNetworkReply* reply)
    : m_headers(reply->rawHeaderPairs())
    , m_sameUrlSequenceNumber(QNetworkReplyInitialSnapshot::getNextSameUrlSequenceNumber(reply->url()))
    , m_url(reply->url())
    , m_streamPosition(0)
{
    m_stream.append(reply->readAll());
    takeSnapshot(QNetworkReplyInitialSnapshot::INITIAL, reply);
}

QNetworkReplyInitialSnapshot::QNetworkReplyInitialSnapshot()
    : m_streamPosition(0)
{
}

QNetworkReplyInitialSnapshot::~QNetworkReplyInitialSnapshot()
{
    foreach (QNetworkReplySnapshotEntry entry, m_snapshots) {
        delete entry.second;
    }
}

QNetworkReplySnapshot* QNetworkReplyInitialSnapshot::takeSnapshot(NetworkSignal signal, QNetworkReply* reply)
{
    m_stream.append(reply->readAll());
    m_snapshots.append(QNetworkReplySnapshotEntry(signal, new QNetworkReplySnapshot(reply, this)));

    return m_snapshots.last().second;
}

QByteArray QNetworkReplyInitialSnapshot::read(qint64 maxlen)
{
    QByteArray chunk = peek(maxlen);
    m_streamPosition += chunk.size();

    return chunk;
}

QByteArray QNetworkReplyInitialSnapshot::peek(qint64 maxlen)
{
    qint64 chunkSize = std::min(maxlen, m_stream.size() - m_streamPosition);

    QByteArray chunk;
    QByteArray::const_iterator it = m_stream.begin() + m_streamPosition;
    for (int i = 0; i < chunkSize; i++) {
        chunk.append((*it));
        ++it;
    }

    return chunk;
}

void QNetworkReplyInitialSnapshot::serialize(QIODevice* stream) const
{
    QDataStream out(stream);
    out << m_headers;
    out << m_sameUrlSequenceNumber;
    out << m_url;
    out << m_stream;

    foreach (const QNetworkReplySnapshotEntry& entry, m_snapshots) {
        out << (int)entry.first;
        out << (int)entry.second->m_error;
        out << entry.second->m_errorString;
        out << entry.second->m_attributes;
        out << entry.second->m_streamSize;
    }

    out << (int)END;
}

QNetworkReplyInitialSnapshot* QNetworkReplyInitialSnapshot::deserialize(QIODevice* stream)
{
    QNetworkReplyInitialSnapshot* initial = new QNetworkReplyInitialSnapshot();

    QDataStream in(stream);

    in >> initial->m_headers
       >> initial->m_sameUrlSequenceNumber
       >> initial->m_url
       >> initial->m_stream;

    while (true) {
        int signal;
        in >> signal;

        if ((NetworkSignal)signal == END) {
            // sentinel
            break;
        }

        QNetworkReplySnapshot* snapshot = new QNetworkReplySnapshot(initial);

        int error;
        in >> error;
        snapshot->m_error = (QNetworkReply::NetworkError)error;

        in >> snapshot->m_errorString;
        in >> snapshot->m_attributes;
        in >> snapshot->m_streamSize;

        initial->m_snapshots.append(QNetworkReplySnapshotEntry((NetworkSignal)signal, snapshot));
    }

    return initial;
}

QHash<QString, unsigned int> QNetworkReplyInitialSnapshot::m_nextSameUrlSequenceNumber;

unsigned int QNetworkReplyInitialSnapshot::getNextSameUrlSequenceNumber(const QUrl& url)
{
    QString urlString = url.toString();
    QHash<QString, unsigned int>::const_iterator iter = QNetworkReplyInitialSnapshot::m_nextSameUrlSequenceNumber.find(urlString);

    unsigned int id = 0;
    if (iter != QNetworkReplyInitialSnapshot::m_nextSameUrlSequenceNumber.end()) {
        id = (*iter);
    }

    QNetworkReplyInitialSnapshot::m_nextSameUrlSequenceNumber.insert(urlString, id+1);

    return id;
}

QNetworkReplySnapshot::QNetworkReplySnapshot(QNetworkReply* reply, QNetworkReplyInitialSnapshot* base)
    : m_base(base)
    , m_error(reply->error())
    , m_errorString(reply->errorString())
{
    m_attributes.insert(QNetworkRequest::HttpStatusCodeAttribute, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute));
    m_attributes.insert(QNetworkRequest::HttpReasonPhraseAttribute, reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute));
    m_attributes.insert(QNetworkRequest::RedirectionTargetAttribute, reply->attribute(QNetworkRequest::RedirectionTargetAttribute));

    m_streamSize = m_base->streamSize();
}

QNetworkReplySnapshot::QNetworkReplySnapshot(QNetworkReplyInitialSnapshot* base)
    : m_base(base)
{
}


QByteArray QNetworkReplySnapshot::rawHeader(const QByteArray &headerName) const
{
    foreach (const QNetworkReply::RawHeaderPair& pair, m_base->m_headers) {
        if (pair.first == headerName) {
            return pair.second;
        }
    }

    return QByteArray();
}

/****************** QNetworkReplyControllable ******************/

QNetworkReplyControllable::QNetworkReplyControllable(QNetworkReply* reply, QObject* parent)
    : QObject(parent)
    , m_sequenceNumber(0)
    , m_nextSnapshotUpdateTimerRunning(false)
    , m_nextSnapshotUpdateTimer(this, &QNetworkReplyControllable::updateSnapshot)
    , m_reply(reply)
    , m_lastNetworkEventAction(0)
{
    Q_ASSERT(m_reply);
}

QNetworkReplyControllable::~QNetworkReplyControllable()
{
    if (m_reply) {
        detachFromReply();
        m_reply->deleteLater();
    }
}

void QNetworkReplyControllable::detachFromReply()
{
    if (m_nextSnapshotUpdateTimer.isActive())
    {
        m_nextSnapshotUpdateTimer.stop();
    }

    m_snapshotQueue.clear();

    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}

QNetworkReply* QNetworkReplyControllable::release()
{
    if (!m_reply)
        return 0;

    detachFromReply();

    QNetworkReply* reply = m_reply;
    m_reply = 0;

    reply->setParent(0);
    return reply;
}

void QNetworkReplyControllable::updateSnapshot(Timer<QNetworkReplyControllable>*)
{
    // HB - Force HB relations between subsequent network events in the same request
    if (m_lastNetworkEventAction != 0) {
        HBAddExplicitArc(m_lastNetworkEventAction, HBCurrentEventAction());
    }

    m_lastNetworkEventAction = HBCurrentEventAction();

    QueuedSnapshot queuedSnapshot = m_snapshotQueue.takeFirst();
    m_currentSnapshot = queuedSnapshot.second;

    m_nextSnapshotUpdateTimerRunning = false;
    scheduleNextSnapshotUpdate();

    switch(queuedSnapshot.first) {
    case QNetworkReplyInitialSnapshot::FINISHED:
        emit finished();
        return; // We are done and this structure has been freed
    case QNetworkReplyInitialSnapshot::READY_READ:
        emit readyRead();
        return; // readyRead() can deallocate this object if the request has finished
    default:
        CRASH();
    }
}

void QNetworkReplyControllable::enqueueSnapshot(QNetworkReplyInitialSnapshot::NetworkSignal signal, QNetworkReplySnapshot* snapshot)
{
    m_snapshotQueue.append(QueuedSnapshot(signal, snapshot));

    scheduleNextSnapshotUpdate();
}

void QNetworkReplyControllable::scheduleNextSnapshotUpdate()
{
    if (m_nextSnapshotUpdateTimerRunning) {
        return; // we are already processing an item from the queue
    }

    if (m_snapshotQueue.empty()) {
        return; // no items in the queue
    }

    m_nextSnapshotUpdateTimerRunning = true;

    QNetworkReplyInitialSnapshot::NetworkSignal signal = m_snapshotQueue.first().first;

    std::stringstream params;
    params << EventActionDescriptor::escapeParam(m_reply->url().toString().toStdString())
           << "," << m_initialSnapshot->getSameUrlSequenceNumber()
           << "," << (signal == QNetworkReplyInitialSnapshot::FINISHED ? ULONG_MAX : m_sequenceNumber++);
    // use ULONG_MAX to indicate the last network event in this sequence

    EventActionDescriptor descriptor("Network", params.str());
    m_nextSnapshotUpdateTimer.setEventActionDescriptor(descriptor);
    m_nextSnapshotUpdateTimer.startOneShot(0);
}

QNetworkReplyControllableLive::QNetworkReplyControllableLive(QNetworkReply* reply, QObject* parent)
    : QNetworkReplyControllable(reply, parent)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(slFinished()));
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(slReadyRead()));

    m_initialSnapshot = new QNetworkReplyInitialSnapshot(reply);
    m_currentSnapshot = m_initialSnapshot->getSnapshots()->first().second;
}

QNetworkReplyControllableLive::~QNetworkReplyControllableLive()
{
}

void QNetworkReplyControllableLive::slFinished()
{
    // this is always handled by the main thread, Qt signal magic
    enqueueSnapshot(QNetworkReplyInitialSnapshot::FINISHED,
                    m_initialSnapshot->takeSnapshot(QNetworkReplyInitialSnapshot::FINISHED, m_reply));
}

void QNetworkReplyControllableLive::slReadyRead()
{
    // this is always handled by the main thread, Qt signal magic
    enqueueSnapshot(QNetworkReplyInitialSnapshot::READY_READ,
                    m_initialSnapshot->takeSnapshot(QNetworkReplyInitialSnapshot::READY_READ, m_reply));
}

void QNetworkReplyControllableLive::detachFromReply()
{
    QNetworkReplyControllable::detachFromReply();

    if (m_reply) {
        m_reply->disconnect(this, SLOT(slFinished()));
        m_reply->disconnect(this, SLOT(slReadyRead()));
    }

    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}

QNetworkReplyControllableFactory* QNetworkReplyControllableFactory::m_factory = new QNetworkReplyControllableFactoryLive();

QNetworkReplyControllableFactory* QNetworkReplyControllableFactory::getFactory()
{
    return QNetworkReplyControllableFactory::m_factory;
}

void QNetworkReplyControllableFactory::setFactory(QNetworkReplyControllableFactory* factory)
{
    delete QNetworkReplyControllableFactory::m_factory;
    QNetworkReplyControllableFactory::m_factory = factory;
}

QNetworkReplyControllableFactory::~QNetworkReplyControllableFactory()
{
}

QNetworkReplyControllableFactoryLive::~QNetworkReplyControllableFactoryLive()
{
}

// WebERA STOP


QNetworkReplyHandlerCallQueue::QNetworkReplyHandlerCallQueue(QNetworkReplyHandler* handler, bool deferSignals)
    : m_replyHandler(handler)
    , m_locks(0)
    , m_deferSignals(deferSignals)
    , m_flushing(false)
{
    Q_ASSERT(handler);
}

void QNetworkReplyHandlerCallQueue::push(EnqueuedCall method)
{
    m_enqueuedCalls.append(method);
    flush();
}

void QNetworkReplyHandlerCallQueue::lock()
{
    ++m_locks;
}

void QNetworkReplyHandlerCallQueue::unlock()
{
    if (!m_locks)
        return;

    --m_locks;
    flush();
}

void QNetworkReplyHandlerCallQueue::setDeferSignals(bool defer, bool sync)
{
    m_deferSignals = defer;
    if (sync)
        flush();
    else
        QMetaObject::invokeMethod(this, "flush",  Qt::QueuedConnection);
}

void QNetworkReplyHandlerCallQueue::flush()
{
    if (m_flushing)
        return;

    m_flushing = true;

    while (!m_deferSignals && !m_locks && !m_enqueuedCalls.isEmpty())
        (m_replyHandler->*(m_enqueuedCalls.takeFirst()))();

    m_flushing = false;
}

class QueueLocker {
public:
    QueueLocker(QNetworkReplyHandlerCallQueue* queue) : m_queue(queue) { m_queue->lock(); }
    ~QueueLocker() { m_queue->unlock(); }
private:
    QNetworkReplyHandlerCallQueue* m_queue;
};

QNetworkReplyWrapper::QNetworkReplyWrapper(QNetworkReplyHandlerCallQueue* queue, QNetworkReply* reply, bool sniffMIMETypes, QObject* parent)
    : QObject(parent)
    , m_reply(QNetworkReplyControllableFactory::getFactory()->construct(reply, this))
    , m_queue(queue)
    , m_responseContainsData(false)
    , m_sniffMIMETypes(sniffMIMETypes)
{
    // setFinished() must be the first that we connect, so isFinished() is updated when running other slots.
    connect(m_reply, SIGNAL(finished()), this, SLOT(setFinished()));
    connect(m_reply, SIGNAL(finished()), this, SLOT(receiveMetaData()));
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(receiveMetaData()));
}

QNetworkReplyWrapper::~QNetworkReplyWrapper()
{
    m_queue->clear();
}

// WebERA: This exposes the non-snapshot version of reply,
// however this is not a problem since, in practice, this is only used for aborting
QNetworkReply* QNetworkReplyWrapper::release()
{
    resetConnections();
    m_sniffer = nullptr;

    return m_reply->release();
}

void QNetworkReplyWrapper::synchronousLoad()
{
    setFinished();
    receiveMetaData();
}

void QNetworkReplyWrapper::resetConnections()
{
    if (m_reply) {
        // Disconnect all connections except the one to setFinished() slot.
        m_reply->disconnect(this, SLOT(receiveMetaData()));
        m_reply->disconnect(this, SLOT(didReceiveFinished()));
        m_reply->disconnect(this, SLOT(didReceiveReadyRead()));
    }
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}

void QNetworkReplyWrapper::receiveMetaData()
{
    // This slot is only used to receive the first signal from the QNetworkReply object.
    resetConnections();


    WTF::String contentType = QString::fromLocal8Bit(m_reply->snapshot()->rawHeader("ContentType"));
    m_encoding = extractCharsetFromMediaType(contentType);
    m_advertisedMIMEType = extractMIMETypeFromMediaType(contentType);

    m_redirectionTargetUrl = m_reply->snapshot()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (m_redirectionTargetUrl.isValid()) {
        QueueLocker lock(m_queue);
        m_queue->push(&QNetworkReplyHandler::sendResponseIfNeeded);
        m_queue->push(&QNetworkReplyHandler::finish);
        return;
    }

    if (!m_sniffMIMETypes) {
        emitMetaDataChanged();
        return;
    }

    bool isSupportedImageType = MIMETypeRegistry::isSupportedImageMIMEType(m_advertisedMIMEType);

    Q_ASSERT(!m_sniffer);

    m_sniffer = adoptPtr(new QtMIMETypeSniffer(m_reply, m_advertisedMIMEType, isSupportedImageType));

    if (m_sniffer->isFinished()) {
        receiveSniffedMIMEType();
        return;
    }

    connect(m_sniffer.get(), SIGNAL(finished()), this, SLOT(receiveSniffedMIMEType()));
}

void QNetworkReplyWrapper::receiveSniffedMIMEType()
{
    Q_ASSERT(m_sniffer);

    m_sniffedMIMEType = m_sniffer->mimeType();
    m_sniffer = nullptr;

    emitMetaDataChanged();
}

void QNetworkReplyWrapper::setFinished()
{
    // Due to a limitation of QNetworkReply public API, its subclasses never get the chance to
    // change the result of QNetworkReply::isFinished() method. So we need to keep track of the
    // finished state ourselves. This limitation is fixed in 4.8, but we'll still have applications
    // that don't use the solution. See http://bugreports.qt.nokia.com/browse/QTBUG-11737.
    Q_ASSERT(!isFinished());
    m_reply->setProperty("_q_isFinished", true);
}

void QNetworkReplyWrapper::emitMetaDataChanged()
{
    QueueLocker lock(m_queue);
    m_queue->push(&QNetworkReplyHandler::sendResponseIfNeeded);

    if (m_reply->snapshot()->bytesAvailable()) {
        m_responseContainsData = true;
        m_queue->push(&QNetworkReplyHandler::forwardData);
    }

    if (isFinished()) {
        m_queue->push(&QNetworkReplyHandler::finish);
        return;
    }

    // If not finished, connect to the slots that will be used from this point on.
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(didReceiveReadyRead()));
    connect(m_reply, SIGNAL(finished()), this, SLOT(didReceiveFinished()));
}

void QNetworkReplyWrapper::didReceiveReadyRead()
{
    if (m_reply->snapshot()->bytesAvailable())
        m_responseContainsData = true;
    m_queue->push(&QNetworkReplyHandler::forwardData);
}

void QNetworkReplyWrapper::didReceiveFinished()
{
    // Disconnecting will make sure that nothing will happen after emitting the finished signal.
    resetConnections();
    m_queue->push(&QNetworkReplyHandler::finish);
}

String QNetworkReplyHandler::httpMethod() const
{
    switch (m_method) {
    case QNetworkAccessManager::GetOperation:
        return "GET";
    case QNetworkAccessManager::HeadOperation:
        return "HEAD";
    case QNetworkAccessManager::PostOperation:
        return "POST";
    case QNetworkAccessManager::PutOperation:
        return "PUT";
    case QNetworkAccessManager::DeleteOperation:
        return "DELETE";
    case QNetworkAccessManager::CustomOperation:
        return m_resourceHandle->firstRequest().httpMethod();
    default:
        ASSERT_NOT_REACHED();
        return "GET";
    }
}

QNetworkReplyHandler::QNetworkReplyHandler(ResourceHandle* handle, LoadType loadType, bool deferred)
    : QObject(0)
    , m_resourceHandle(handle)
    , m_loadType(loadType)
    , m_redirectionTries(gMaxRedirections)
    , m_queue(this, deferred)
{
    const ResourceRequest &r = m_resourceHandle->firstRequest();

    if (r.httpMethod() == "GET")
        m_method = QNetworkAccessManager::GetOperation;
    else if (r.httpMethod() == "HEAD")
        m_method = QNetworkAccessManager::HeadOperation;
    else if (r.httpMethod() == "POST")
        m_method = QNetworkAccessManager::PostOperation;
    else if (r.httpMethod() == "PUT")
        m_method = QNetworkAccessManager::PutOperation;
    else if (r.httpMethod() == "DELETE")
        m_method = QNetworkAccessManager::DeleteOperation;
    else
        m_method = QNetworkAccessManager::CustomOperation;

    m_request = r.toNetworkRequest(m_resourceHandle->getInternal()->m_context.get());

    m_queue.push(&QNetworkReplyHandler::start);
}

void QNetworkReplyHandler::abort()
{
    m_resourceHandle = 0;
    if (QNetworkReply* reply = release()) {
        reply->abort();
        reply->deleteLater();
    }
    deleteLater();
}

QNetworkReply* QNetworkReplyHandler::release()
{
    if (!m_replyWrapper)
        return 0;

    QNetworkReply* reply = m_replyWrapper->release();
    m_replyWrapper = nullptr;
    return reply;
}

static bool shouldIgnoreHttpError(QNetworkReplyControllable* reply, bool receivedData)
{
    int httpStatusCode = reply->snapshot()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (httpStatusCode == 401 || httpStatusCode == 407)
        return true;

    if (receivedData && (httpStatusCode >= 400 && httpStatusCode < 600))
        return true;

    return false;
}

void QNetworkReplyHandler::finish()
{
    ASSERT(m_replyWrapper && m_replyWrapper->reply() && !wasAborted());

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client) {
        m_replyWrapper = nullptr;
        return;
    }

    if (m_replyWrapper->wasRedirected()) {
        m_replyWrapper = nullptr;
        m_queue.push(&QNetworkReplyHandler::start);
        return;
    }

    if (!m_replyWrapper->reply()->snapshot()->error() || shouldIgnoreHttpError(m_replyWrapper->reply(), m_replyWrapper->responseContainsData()))
        client->didFinishLoading(m_resourceHandle, 0);
    else
        client->didFail(m_resourceHandle, errorForReply(m_replyWrapper->reply()));

    m_replyWrapper = nullptr;
}

void QNetworkReplyHandler::sendResponseIfNeeded()
{
    ASSERT(m_replyWrapper && m_replyWrapper->reply() && !wasAborted());

    if (m_replyWrapper->reply()->snapshot()->error() && m_replyWrapper->reply()->snapshot()->attribute(QNetworkRequest::HttpStatusCodeAttribute).isNull())
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    WTF::String mimeType = m_replyWrapper->mimeType();

    if (mimeType.isEmpty()) {
        // let's try to guess from the extension
        mimeType = MIMETypeRegistry::getMIMETypeForPath(m_replyWrapper->reply()->snapshot()->url().path());
    }

    KURL url(m_replyWrapper->reply()->snapshot()->url());
    ResourceResponse response(url, mimeType.lower(),
                              QString::fromLocal8Bit(m_replyWrapper->reply()->snapshot()->rawHeader("ContentLength")).toLongLong(),
                              m_replyWrapper->encoding(), String());

    if (url.isLocalFile()) {
        client->didReceiveResponse(m_resourceHandle, response);
        return;
    }

    // The status code is equal to 0 for protocols not in the HTTP family.
    int statusCode = m_replyWrapper->reply()->snapshot()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (url.protocolIsInHTTPFamily()) {
        String suggestedFilename = filenameFromHTTPContentDisposition(QString::fromLatin1(m_replyWrapper->reply()->snapshot()->rawHeader("Contentisposition")));

        if (!suggestedFilename.isEmpty())
            response.setSuggestedFilename(suggestedFilename);
        else
            response.setSuggestedFilename(url.lastPathComponent());

        response.setHTTPStatusCode(statusCode);
        response.setHTTPStatusText(m_replyWrapper->reply()->snapshot()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray().constData());

        // Add remaining headers.
        foreach (const QNetworkReply::RawHeaderPair& pair, m_replyWrapper->reply()->snapshot()->rawHeaderPairs())
            response.setHTTPHeaderField(QString::fromLatin1(pair.first), QString::fromLatin1(pair.second));
    }

    QUrl redirection = m_replyWrapper->reply()->snapshot()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirection.isValid()) {
        redirect(response, redirection);
        return;
    }

    client->didReceiveResponse(m_resourceHandle, response);
}

void QNetworkReplyHandler::redirect(ResourceResponse& response, const QUrl& redirection)
{
    QUrl newUrl = m_replyWrapper->reply()->snapshot()->url().resolved(redirection);

    ResourceHandleClient* client = m_resourceHandle->client();
    ASSERT(client);

    int statusCode = m_replyWrapper->reply()->snapshot()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    m_redirectionTries--;
    if (!m_redirectionTries) {
        ResourceError error("HTTP", 400 /*bad request*/,
                            newUrl.toString(),
                            QCoreApplication::translate("QWebPage", "Redirection limit reached"));
        client->didFail(m_resourceHandle, error);
        m_replyWrapper = nullptr;
        return;
    }

    //  Status Code 301 (Moved Permanently), 302 (Moved Temporarily), 303 (See Other):
    //    - If original request is POST convert to GET and redirect automatically
    //  Status Code 307 (Temporary Redirect) and all other redirect status codes:
    //    - Use the HTTP method from the previous request
    if ((statusCode >= 301 && statusCode <= 303) && m_resourceHandle->firstRequest().httpMethod() == "POST")
        m_method = QNetworkAccessManager::GetOperation;

    ResourceRequest newRequest = m_resourceHandle->firstRequest();
    newRequest.setHTTPMethod(httpMethod());
    newRequest.setURL(newUrl);

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!newRequest.url().protocolIs("https") && protocolIs(newRequest.httpReferrer(), "https"))
        newRequest.clearHTTPReferrer();

    client->willSendRequest(m_resourceHandle, newRequest, response);
    if (wasAborted()) // Network error cancelled the request.
        return;

    m_request = newRequest.toNetworkRequest(m_resourceHandle->getInternal()->m_context.get());
}

void QNetworkReplyHandler::forwardData()
{
    ASSERT(m_replyWrapper && m_replyWrapper->reply() && !wasAborted() && !m_replyWrapper->wasRedirected());

    QByteArray data = m_replyWrapper->reply()->snapshot()->read(m_replyWrapper->reply()->snapshot()->bytesAvailable());

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=19793
    // -1 means we do not provide any data about transfer size to inspector so it would use
    // Content-Length headers or content size to show transfer size.
    if (!data.isEmpty())
        client->didReceiveData(m_resourceHandle, data.constData(), data.length(), -1);
}

void QNetworkReplyHandler::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (wasAborted())
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    client->didSendData(m_resourceHandle, bytesSent, bytesTotal);
}

void QNetworkReplyHandler::clearContentHeaders()
{
    // Clearing Content-length and Content-type of the requests that do not have contents.
    // This is necessary to ensure POST requests redirected to GETs do not leak metadata
    // about the POST content to the site they've been redirected to.
    m_request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());
    m_request.setHeader(QNetworkRequest::ContentLengthHeader, QVariant());
}

FormDataIODevice* QNetworkReplyHandler::getIODevice(const ResourceRequest& request)
{
    FormDataIODevice* device = new FormDataIODevice(request.httpBody());
    // We may be uploading files so prevent QNR from buffering data.
    m_request.setHeader(QNetworkRequest::ContentLengthHeader, device->getFormDataSize());
    m_request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, QVariant(true));
    return device;
}

QNetworkReply* QNetworkReplyHandler::sendNetworkRequest(QNetworkAccessManager* manager, const ResourceRequest& request)
{
    if (m_loadType == SynchronousLoad)
        m_request.setAttribute(gSynchronousNetworkRequestAttribute, true);

    if (!manager)
        return 0;

    const QUrl url = m_request.url();

    // Post requests on files and data don't really make sense, but for
    // fast/forms/form-post-urlencoded.html and for fast/forms/button-state-restore.html
    // we still need to retrieve the file/data, which means we map it to a Get instead.
    if (m_method == QNetworkAccessManager::PostOperation
        && (!url.toLocalFile().isEmpty() || url.scheme() == QLatin1String("data")))
        m_method = QNetworkAccessManager::GetOperation;

    switch (m_method) {
        case QNetworkAccessManager::GetOperation:
            clearContentHeaders();
            return manager->get(m_request);
        case QNetworkAccessManager::PostOperation: {
            FormDataIODevice* postDevice = getIODevice(request);
            QNetworkReply* result = manager->post(m_request, postDevice);
            postDevice->setParent(result);
            return result;
        }
        case QNetworkAccessManager::HeadOperation:
            clearContentHeaders();
            return manager->head(m_request);
        case QNetworkAccessManager::PutOperation: {
            FormDataIODevice* putDevice = getIODevice(request);
            QNetworkReply* result = manager->put(m_request, putDevice);
            putDevice->setParent(result);
            return result;
        }
        case QNetworkAccessManager::DeleteOperation: {
            clearContentHeaders();
            return manager->deleteResource(m_request);
        }
        case QNetworkAccessManager::CustomOperation: {
            FormDataIODevice* customDevice = getIODevice(request);
            QNetworkReply* result = manager->sendCustomRequest(m_request, m_resourceHandle->firstRequest().httpMethod().latin1().data(), customDevice);
            customDevice->setParent(result);
            return result;
        }
        case QNetworkAccessManager::UnknownOperation:
            ASSERT_NOT_REACHED();
            return 0;
    }
    return 0;
}

void QNetworkReplyHandler::start()
{
    ResourceHandleInternal* d = m_resourceHandle->getInternal();
    if (!d || !d->m_context)
        return;

    QNetworkReply* reply = sendNetworkRequest(d->m_context->networkAccessManager(), d->m_firstRequest);
    if (!reply)
        return;

    m_replyWrapper = adoptPtr(new QNetworkReplyWrapper(&m_queue, reply, m_resourceHandle->shouldContentSniff() && d->m_context->mimeSniffingEnabled(), this));

    if (m_loadType == SynchronousLoad) {
        m_replyWrapper->synchronousLoad();
        // If supported, a synchronous request will be finished at this point, no need to hook up the signals.
        return;
    }

    if (m_resourceHandle->firstRequest().reportUploadProgress())
        connect(m_replyWrapper->reply(), SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(uploadProgress(qint64, qint64)));
}

ResourceError QNetworkReplyHandler::errorForReply(QNetworkReplyControllable* reply)
{
    QUrl url = reply->snapshot()->url();
    int httpStatusCode = reply->snapshot()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (httpStatusCode)
        return ResourceError("HTTP", httpStatusCode, url.toString(), reply->snapshot()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());

    return ResourceError("QtNetwork", reply->snapshot()->error(), url.toString(), reply->snapshot()->errorString());
}

}

#include "moc_QNetworkReplyHandler.cpp"
