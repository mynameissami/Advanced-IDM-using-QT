#include "downloaditem.h"
#include <QNetworkRequest>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QThread>
#include <algorithm>

DownloadItem::DownloadItem(const QUrl &url, const QString &filePath, QObject *parent)
    : QObject(parent), m_url(url), m_fullFilePath(filePath), m_totalSize(-1), m_downloadedSize(0),
    m_state(Queued), m_reply(nullptr), m_manager(new QNetworkAccessManager(this)), m_transferRate(0),
    m_speedLimit(0), m_bytesReadThisSecond(0), m_numChunks(1), m_supportsRange(true), m_file(nullptr),
    m_isSingleChunk(false), m_lastUpdateTime(QDateTime::currentMSecsSinceEpoch()), m_chunkProgress(nullptr)
{
    m_chunkProgress = new qint64[1]; // Allocate array
    m_chunkProgress[0] = 0;
    m_speedLimitTimer.start();
    m_rateTimer = new QTimer(this);
    connect(m_rateTimer, &QTimer::timeout, this, &DownloadItem::updateTransferRate);
    m_rateTimer->start(1000); // Update rate every second

    QString fileName = QFileInfo(filePath).fileName();
    if (fileName.isEmpty()) {
        fileName = "download_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    }
    setFileName(fileName);
}

/**
 * Destructor for the DownloadItem class.
 * Stops any ongoing download, terminates the worker thread, and cleans up resources.
 */
DownloadItem::~DownloadItem()
{
    stop();
    if (m_workerThread) {
        m_workerThread->quit(); // Request thread to stop
        if (!m_workerThread->wait(5000)) { // Wait up to 5 seconds
            qWarning() << "Worker thread did not stop for" << m_fileName << ", forcing termination";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    delete m_worker;
    m_worker = nullptr;
    delete m_manager; // Direct deletion to ensure cleanup
    delete[] m_chunkProgress;
}

/**
 * @brief Creates a QNetworkRequest object with custom attributes set.
 */
QNetworkRequest DownloadItem::createNetworkRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/91.0.4472.124 Safari/537.36 Edg/91.0.864.59");
    return request;
}

/**
 * @brief Sets the state of the DownloadItem and emits a stateChanged signal if the state changes.
 */
void DownloadItem::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}
void DownloadItem::setNumChunks(int numChunks) {
    if (numChunks < 1) numChunks = 1;
    if (numChunks == m_numChunks) return;
    delete[] m_chunkProgress;
    m_numChunks = numChunks;
    m_chunkProgress = new qint64[numChunks]();
    // Distribute downloadedSize across chunks if needed
    if (m_downloadedSize > 0 && m_totalSize > 0) {
        qint64 perChunk = m_downloadedSize / numChunks;
        for (int i = 0; i < numChunks; ++i) {
            m_chunkProgress[i] = (i == numChunks - 1) ? (m_downloadedSize - (perChunk * (numChunks - 1))) : perChunk;
        }
    }
}

/**
 * @brief Sets the network proxy for the DownloadItem.
 */
void DownloadItem::setProxy(const QNetworkProxy &proxy)
{
    if (m_manager) m_manager->setProxy(proxy);
}

/**
 * @brief Sets the speed limit of the DownloadItem in bytes per second.
 */
void DownloadItem::setSpeedLimit(qint64 bytesPerSec)
{
    QMutexLocker locker(&m_speedLimitMutex);
    qDebug() << "Setting speed limit to" << bytesPerSec << "for" << m_fileName
             << "(state:" << m_state << ", thread:" << QThread::currentThreadId() << ")";
    if (m_state == Failed || m_state == Completed) {
        qWarning() << "Ignoring speed limit change for" << m_fileName << "in state" << m_state;
        return;
    }
    m_speedLimit = bytesPerSec;
    m_bytesReadThisSecond = 0;
    m_speedLimitTimer.restart();

    if (!m_worker) {
        m_worker = new SpeedLimitWorker(this);
        m_workerThread = new QThread(this);
        m_worker->moveToThread(m_workerThread);
        connect(m_workerThread, &QThread::started, m_worker, [this]() {
            QMetaObject::invokeMethod(m_worker, "enforceLimit", Qt::QueuedConnection, Q_ARG(qint64, 0));
        }, Qt::QueuedConnection);
        m_workerThread->start();
        qDebug() << "Worker thread started for" << m_fileName;
    }
}

void DownloadItem::enforceSpeedLimit(qint64 bytesToRead)
{
    if (m_speedLimit <= 0 || !m_worker || !m_workerThread || !m_workerThread->isRunning()) {
        qDebug() << "No speed limit or worker unavailable for" << m_fileName;
        return;
    }

    QMutexLocker locker(&m_speedLimitMutex);
    qint64 elapsed = m_speedLimitTimer.elapsed();
    m_bytesReadThisSecond += bytesToRead;

    if (elapsed >= 1000) {
        m_bytesReadThisSecond = 0; // Reset fully at second boundary
        m_speedLimitTimer.restart();
    } else if (m_bytesReadThisSecond > m_speedLimit) {
        qint64 excess = m_bytesReadThisSecond - m_speedLimit;
        qint64 sleepTime = (excess * 1000) / qMax<qint64>(1, m_speedLimit);
        if (sleepTime > 0) {
            qDebug() << "Throttling" << m_fileName << "sleeping for" << sleepTime << "ms, excess:" << excess;
            QThread::msleep(static_cast<quint64>(sleepTime));
            m_bytesReadThisSecond = bytesToRead; // Reset to current read after sleep
        }
    }

    QMetaObject::invokeMethod(m_worker, "enforceLimit", Qt::QueuedConnection, Q_ARG(qint64, bytesToRead));
}

SpeedLimitWorker::SpeedLimitWorker(DownloadItem* parent) : m_parent(parent), m_timer(new QElapsedTimer())
{
    m_timer->start();
    if (m_parent) {
        qDebug() << "SpeedLimitWorker created for" << m_parent->m_fileName << "in thread" << QThread::currentThreadId();
    } else {
        qDebug() << "SpeedLimitWorker created with null parent in thread" << QThread::currentThreadId();
    }
}

SpeedLimitWorker::~SpeedLimitWorker()
{
    delete m_timer;
    if (m_parent) {
        qDebug() << "SpeedLimitWorker destroyed for" << m_parent->m_fileName;
    } else {
        qDebug() << "SpeedLimitWorker destroyed with null parent";
    }
}

void SpeedLimitWorker::enforceLimit(qint64 bytesToRead)
{
    if (!m_parent || m_parent->m_speedLimit <= 0) {
        qDebug() << "No speed limit or parent invalid for" << (m_parent ? m_parent->m_fileName : "unknown");
        return;
    }

    QMutexLocker locker(&m_parent->m_speedLimitMutex);
    qint64 elapsed = m_timer->elapsed();
    qint64 totalBytes = m_parent->m_bytesReadThisSecond + bytesToRead;

    qDebug() << "Enforcing limit: elapsed=" << elapsed << ", bytesToRead=" << bytesToRead
             << ", bytesReadThisSecond=" << m_parent->m_bytesReadThisSecond << ", totalBytes=" << totalBytes;

    if (elapsed >= 1000) {
        m_parent->m_bytesReadThisSecond = bytesToRead;
        m_timer->restart();
    } else if (totalBytes > m_parent->m_speedLimit) {
        qint64 excess = totalBytes - m_parent->m_speedLimit;
        qint64 sleepTime = (excess * 1000) / qMax<qint64>(1, m_parent->m_speedLimit);
        if (sleepTime > 0) {
            qDebug() << "Worker throttling" << m_parent->m_fileName << "sleeping for" << sleepTime << "ms";
            QThread::msleep(static_cast<quint64>(sleepTime));
            m_parent->m_bytesReadThisSecond = bytesToRead;
        }
    } else {
        m_parent->m_bytesReadThisSecond = totalBytes;
    }
}

// This will start download and file in the current queue, except for which is completed or Downloading
void DownloadItem::start()
{
    if (m_state == Downloading || m_state == Completed) return;

    if (!m_url.isValid()) {
        setState(Failed);
        emit failed("Invalid URL provided");
        return;
    }

    QFileInfo fileInfo(m_fullFilePath);
    QDir dir = fileInfo.absoluteDir();
    setFullFilePath(m_fullFilePath);
    qDebug () << "File Path"<<dir;
    if (!dir.exists() && !dir.mkpath(".")) {
        setState(Failed);
        emit failed("Could not create download directory");
        return;
    }

    setState(Downloading);
    setLastTryDate(QDateTime::currentDateTime());
    fetchTotalSize();
}

void DownloadItem::fetchTotalSize()
{
    QNetworkRequest request = createNetworkRequest(m_url);
    m_reply = m_manager->head(request);
    if (!m_reply) {
        setState(Failed);
        emit failed("Failed to initiate download request");
        return;
    }

    connect(m_reply, &QNetworkReply::finished, this, &DownloadItem::onHeadFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadItem::onError);
}

/**
 * Handles the completion of the HEAD request.
 */
void DownloadItem::onHeadFinished()
{
    if (!m_reply) {
        qCritical() << "Null reply in onHeadFinished";
        setState(Failed);
        emit failed("Null network reply");
        return;
    }

    if (m_reply->error() == QNetworkReply::NoError) {
        m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        m_supportsRange = m_reply->rawHeader("Accept-Ranges").toLower().contains("bytes");
        m_isSingleChunk = !m_supportsRange || m_totalSize <= 0;
        m_numChunks = m_isSingleChunk ? 1 : qBound(4, (int)(m_totalSize / (5 * 1024 * 1024)), 16);
        qDebug() << "onHeadFinished: totalSize=" << m_totalSize << ", supportsRange=" << m_supportsRange << ", isSingleChunk=" << m_isSingleChunk;
    } else {
        qWarning() << "HEAD request failed:" << m_reply->errorString() << ", falling back to GET";
        m_isSingleChunk = true;
        m_numChunks = 1;
        fetchSizeWithGet(); // Fallback to GET if HEAD fails
    }

    m_reply->deleteLater();
    m_reply = nullptr;
    if (m_state != Failed && !m_isSingleChunk) startChunkDownloads();
    else if (m_state != Failed) startSingleChunkDownload();
}

/**
 * Initiates a GET request to estimate the file size as a fallback.
 */
void DownloadItem::fetchSizeWithGet()
{
    if (!m_url.isValid()) {
        qDebug() << "Invalid URL in GET fallback:" << m_url;
        setState(Failed);
        emit failed("Invalid URL in GET fallback");
        return;
    }
    QNetworkRequest request = createNetworkRequest(m_url);
    m_reply = m_manager->get(request);
    if (!m_reply) {
        setState(Failed);
        emit failed("Failed to initiate GET request for size estimation");
        return;
    }
    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadItem::onGetReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadItem::onGetFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadItem::onError);
}

void DownloadItem::onGetReadyRead()
{
    if (!m_reply) return;
    if (m_totalSize <= 0) {
        m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (m_totalSize <= 0) {
            m_totalSize = m_reply->bytesAvailable() + m_downloadedSize;
            m_reply->abort(); // Stop reading further to save bandwidth
        }
    }
}

void DownloadItem::onGetFinished()
{
    if (!m_reply) return;

    if (m_reply->error() != QNetworkReply::NoError && m_totalSize <= 0) {
        m_totalSize = m_downloadedSize + m_reply->bytesAvailable();
    } else if (m_totalSize <= 0) {
        m_totalSize = m_downloadedSize + m_reply->bytesAvailable();
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_state != Failed && m_totalSize > 0) {
        m_isSingleChunk = true; // Fallback to single chunk after GET
        startChunkDownloads();
    } else {
        setState(Failed);
        emit failed("Could not determine file size");
    }
}

void DownloadItem::startChunkDownloads()
{
    if (m_isSingleChunk) {
        startSingleChunkDownload();
        return;
    }

    initializeChunks();
    checkPartialChunks();

    for (int i = 0; i < m_numChunks; ++i) {
        startOrResumeChunk(i);
    }
    emit progress(m_downloadedSize, m_totalSize);
}

void DownloadItem::startSingleChunkDownload()
{
    if (!m_url.isValid() || m_state != Downloading) return;

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    m_file = new QFile(m_fullFilePath);
    if (!m_file || (!m_file->open(QFile::exists(m_fullFilePath) ? QIODevice::Append : QIODevice::WriteOnly))) {
        qCritical() << "Failed to open file:" << m_fullFilePath << m_file->errorString();
        delete m_file;
        m_file = nullptr;
        setState(Failed);
        emit failed("Could not open output file: " + m_file->errorString());
        return;
    }

    m_downloadedSize = QFile::exists(m_fullFilePath) ? m_file->size() : 0;
    QNetworkRequest request = createNetworkRequest(m_url);
    if (m_downloadedSize > 0) request.setRawHeader("Range", QString("bytes=%1-").arg(m_downloadedSize).toUtf8());

    m_reply = m_manager->get(request);
    if (!m_reply) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
        setState(Failed);
        emit failed("Failed to initiate download");
        return;
    }

    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadItem::onSingleChunkReadyRead, Qt::UniqueConnection);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadItem::onSingleChunkFinished, Qt::UniqueConnection);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadItem::onError, Qt::UniqueConnection);
}

void DownloadItem::onSingleChunkReadyRead()
{
    if (!m_reply || !m_file || !m_file->isOpen()) {
        qCritical() << "Invalid state in onSingleChunkReadyRead: reply=" << m_reply << ", file=" << m_file;
        if (m_reply) m_reply->abort();
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        setState(Failed);
        emit failed("Invalid file or reply state");
        return;
    }

    qint64 maxRead = m_speedLimit > 0 ? qMin(m_reply->bytesAvailable(), m_speedLimit / 100) : m_reply->bytesAvailable();
    QByteArray data = m_reply->read(maxRead);
    if (data.isEmpty()) {
        qDebug() << "No data received, checking reply error:" << m_reply->errorString();
        return;
    }

    enforceSpeedLimit(data.size()); // Enforce limit before writing
    qint64 bytesWritten = m_file->write(data);
    if (bytesWritten <= 0) {
        qCritical() << "Write failed:" << m_file->errorString() << "for" << m_fileName;
        m_file->close();
        delete m_file;
        m_file = nullptr;
        setState(Failed);
        emit failed("Failed to write to file: " + m_file->errorString());
        return;
    }

    m_downloadedSize += bytesWritten;
    if (m_totalSize <= 0) {
        QVariant contentLength = m_reply->header(QNetworkRequest::ContentLengthHeader);
        m_totalSize = contentLength.isValid() ? contentLength.toLongLong() : m_downloadedSize;
    }
    emit progress(m_downloadedSize, m_totalSize > 0 ? m_totalSize : m_downloadedSize);
}
void DownloadItem::onSingleChunkFinished()
{
    if (!m_reply) {
        qCritical() << "Null reply in onSingleChunkFinished";
        return;
    }

    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
    if (m_reply->error() == QNetworkReply::NoError) {
        if (m_totalSize <= 0) m_totalSize = m_downloadedSize;
        setState(Completed);
        emit finished();
    } else if (m_reply->error() != QNetworkReply::OperationCanceledError) {
        setState(Failed);
        emit failed(m_reply->errorString());
    }

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    delete m_file;
    m_file = nullptr;
}

void DownloadItem::pause()
{
    if (m_state != Downloading) {
        qDebug() << "pause: Item" << m_fileName << "not in Downloading state, current state:" << m_state;
        return;
    }

    qDebug() << "Pausing HTTP download:" << m_fileName;
    setState(Paused);
    m_rateTimer->stop();

    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_isSingleChunk && m_file) {
        if (m_file->isOpen()) {
            m_file->flush();
            m_file->close();
        }
        delete m_file;
        m_file = nullptr;
    } else {
        cleanup(false);
    }

    emit progress(m_downloadedSize, m_totalSize);
    qDebug() << "Paused HTTP download:" << m_fileName << "Downloaded:" << m_downloadedSize << "Total:" << m_totalSize;
}

void DownloadItem::resume()
{
    if (m_state != Paused) return;

    setState(Downloading);
    m_rateTimer->start(1000);
    if (m_isSingleChunk) {
        startSingleChunkDownload();
    } else {
        initializeChunks();
        checkPartialChunks();
        for (int i = 0; i < m_numChunks; ++i) {
            startOrResumeChunk(i);
        }
    }
    emit progress(m_downloadedSize, m_totalSize);
}

void DownloadItem::stop()
{
    if (m_state == Stopped || m_state == Completed || m_state == Failed || m_state == Paused) return;

    setState(Stopped);
    m_rateTimer->stop();
    cleanup(true);
    emit progress(m_downloadedSize, m_totalSize);
}

void DownloadItem::cleanup(bool deleteFiles)
{
    qDebug() << "Cleaning up for" << m_fileName << "Delete files:" << deleteFiles;
    if (m_file) {
        if (m_file->isOpen()) {
            m_file->flush();
            m_file->close();
        }
        if (deleteFiles) {
            m_file->remove();
        }
        delete m_file;
        m_file = nullptr;
    }

    for (int i = 0; i < m_chunkReplies.size(); ++i) {
        if (m_chunkReplies[i]) {
            disconnect(m_chunkReplies[i], nullptr, this, nullptr); // Disconnect all signals
            m_chunkReplies[i]->abort();
            m_chunkReplies[i]->deleteLater();
            m_chunkReplies[i] = nullptr;
        }
    }
    m_chunkReplies.clear();

    for (int i = 0; i < m_chunkFiles.size(); ++i) {
        if (m_chunkFiles[i]) {
            if (m_chunkFiles[i]->isOpen()) {
                m_chunkFiles[i]->flush();
                m_chunkFiles[i]->close();
            }
            if (deleteFiles) {
                m_chunkFiles[i]->remove();
            }
            delete m_chunkFiles[i];
            m_chunkFiles[i] = nullptr;
        }
    }
    m_chunkFiles.clear();
    m_chunkDownloaded.clear();

    QMutexLocker locker(&m_speedLimitMutex);
    m_bytesReadThisSecond = 0;
    m_speedLimitTimer.restart();
}

bool DownloadItem::checkPartialChunks()
{
    m_downloadedSize = 0;
    for (int i = 0; i < m_numChunks; ++i) {
        QString chunkFileName = QString("%1.chunk%2").arg(m_fullFilePath).arg(i);
        QFile chunkFile(chunkFileName);
        if (chunkFile.exists()) {
            m_chunkDownloaded[i] = chunkFile.size();
            m_downloadedSize += m_chunkDownloaded[i];
        }
    }
    return m_downloadedSize > 0;
}

void DownloadItem::initializeChunks()
{
    m_isSingleChunk = m_totalSize <= 0 || !m_supportsRange;
    m_numChunks = m_isSingleChunk ? 1 : qBound(4, (int)(m_totalSize / (5 * 1024 * 1024)), 16);

    m_chunks.resize(m_numChunks + 1);
    m_chunkReplies.resize(m_numChunks);
    m_chunkFiles.resize(m_numChunks);
    m_chunkDownloaded.resize(m_numChunks);
    m_chunkReplies.fill(nullptr);
    m_chunkFiles.fill(nullptr);
    m_chunkDownloaded.fill(0);

    if (m_totalSize > 0) {
        qint64 chunkSize = m_totalSize / m_numChunks;
        for (int i = 0; i < m_numChunks; ++i) {
            m_chunks[i] = i * chunkSize;
        }
        m_chunks[m_numChunks] = m_totalSize;
    }
}

void DownloadItem::startOrResumeChunk(int chunkIndex)
{
    QMutexLocker locker(&m_chunkMutex);
    qint64 startOffset = m_chunks[chunkIndex];
    qint64 endOffset = m_chunks[chunkIndex + 1] - 1;
    qint64 chunkTotalSize = endOffset - startOffset + 1;

    if (m_totalSize > 0 && m_chunkDownloaded[chunkIndex] >= chunkTotalSize) return;

    QNetworkRequest request = createNetworkRequest(m_url);
    if (m_supportsRange && m_totalSize > 0) {
        QString rangeHeader = QString("bytes=%1-%2").arg(startOffset + m_chunkDownloaded[chunkIndex]).arg(endOffset);
        request.setRawHeader("Range", rangeHeader.toUtf8());
    }

    m_chunkReplies[chunkIndex] = m_manager->get(request);
    if (!m_chunkReplies[chunkIndex]) {
        setState(Failed);
        emit failed("Failed to initiate chunk download");
        return;
    }

    if (!m_chunkFiles[chunkIndex]) {
        m_chunkFiles[chunkIndex] = new QFile(QString("%1.chunk%2").arg(m_fullFilePath).arg(chunkIndex));
        if (!m_chunkFiles[chunkIndex]->open(QIODevice::Append)) {
            delete m_chunkFiles[chunkIndex];
            m_chunkFiles[chunkIndex] = nullptr;
            m_chunkReplies[chunkIndex]->abort();
            m_chunkReplies[chunkIndex] = nullptr;
            setState(Failed);
            emit failed("Could not open chunk file");
            return;
        }
    }

    connect(m_chunkReplies[chunkIndex], &QNetworkReply::readyRead, this, [this, chunkIndex]() { onChunkReadyRead(chunkIndex); });
    connect(m_chunkReplies[chunkIndex], &QNetworkReply::finished, this, [this, chunkIndex]() { onChunkFinished(chunkIndex); });
    connect(m_chunkReplies[chunkIndex], &QNetworkReply::errorOccurred, this, &DownloadItem::onError);
}

void DownloadItem::onChunkReadyRead(int chunkIndex)
{
    if (!m_chunkReplies[chunkIndex] || !m_chunkFiles[chunkIndex]) return;

    qint64 maxRead = m_speedLimit > 0 ? qMin(m_chunkReplies[chunkIndex]->bytesAvailable(), m_speedLimit / 100) : m_chunkReplies[chunkIndex]->bytesAvailable();
    QByteArray data = m_chunkReplies[chunkIndex]->read(maxRead);
    if (data.isEmpty()) return;

    enforceSpeedLimit(data.size()); // Enforce limit per chunk
    qint64 bytesWritten = m_chunkFiles[chunkIndex]->write(data);
    if (bytesWritten > 0) {
        m_chunkDownloaded[chunkIndex] += bytesWritten;
        m_downloadedSize += bytesWritten;
        emit progress(m_downloadedSize, m_totalSize > 0 ? m_totalSize : m_downloadedSize);
    }
}

void DownloadItem::onChunkFinished(int chunkIndex)
{
    QNetworkReply *reply = m_chunkReplies[chunkIndex];
    if (!reply) return;

    if (m_chunkFiles[chunkIndex] && m_chunkFiles[chunkIndex]->isOpen()) m_chunkFiles[chunkIndex]->close();
    if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
        setState(Failed);
        emit failed(reply->errorString());
        cleanup(false);
    } else {
        m_chunkReplies[chunkIndex] = nullptr;
        bool allDone = std::all_of(m_chunkReplies.begin(), m_chunkReplies.end(), [](QNetworkReply *r) { return !r; });
        if (allDone && m_state == Downloading) mergeChunks();
    }
    reply->deleteLater();
}

void DownloadItem::mergeChunks()
{
    QFile finalFile(m_fullFilePath);
    if (!finalFile.open(QIODevice::WriteOnly)) {
        setState(Failed);
        emit failed("Cannot open final file for merging");
        return;
    }

    for (int i = 0; i < m_numChunks; ++i) {
        if (m_chunkFiles[i] && m_chunkFiles[i]->open(QIODevice::ReadOnly)) {
            finalFile.write(m_chunkFiles[i]->readAll());
            m_chunkFiles[i]->close();
        } else {
            setState(Failed);
            emit failed(QString("Could not read chunk %1 for merging").arg(i));
            finalFile.remove();
            return;
        }
    }

    finalFile.close();
    cleanup(true);
    setState(Completed);
    emit finished();
}

void DownloadItem::onError(QNetworkReply::NetworkError code)
{
    if (code != QNetworkReply::OperationCanceledError) {
        setState(Failed);
        emit failed(m_reply ? m_reply->errorString() : "Network error");
    }
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    for (QNetworkReply *reply : m_chunkReplies) {
        if (reply) {
            reply->deleteLater();
            reply = nullptr;
        }
    }
}

void DownloadItem::updateTransferRate()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeDiff = currentTime - m_lastUpdateTime;
    if (timeDiff > 0) {
        qint64 bytesDiff = m_downloadedSize - m_bytesLastPeriod;
        m_transferRate = (bytesDiff * 1000) / timeDiff;
        m_bytesLastPeriod = m_downloadedSize;
        m_lastUpdateTime = currentTime;
    }
}

qint64 DownloadItem::getChunkProgress(int chunkIndex) const {
    if (chunkIndex < 0 || chunkIndex >= m_numChunks || !m_chunkProgress) return 0;
    return m_chunkProgress[chunkIndex];
}

qint64 DownloadItem::getCurrentSpeedLimit()
{
    QMutexLocker locker(&m_speedLimitMutex);
    return m_speedLimit;
}

bool DownloadItem::validateChunk(int chunkIndex)
{
    if (chunkIndex < 0 || chunkIndex >= m_numChunks) return false;
    QString chunkFileName = QString("%1.chunk%2").arg(m_fullFilePath).arg(chunkIndex);
    QFile chunkFile(chunkFileName);
    return chunkFile.exists() && chunkFile.size() == m_chunkDownloaded[chunkIndex];
}

void DownloadItem::setFullFilePath(const QString &path)
{
    m_fullFilePath = path;
    qDebug() << "Setting full file path for" << m_fileName << "to" << path;
}
