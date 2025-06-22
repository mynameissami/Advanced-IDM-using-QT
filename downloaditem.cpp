#include "downloaditem.h"
#include <QNetworkRequest>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QThread>

DownloadItem::DownloadItem(const QUrl &url, const QString &filePath, QObject *parent)
    : QObject(parent), m_url(url), m_fullFilePath(filePath), m_totalSize(-1), m_downloadedSize(0),
    m_state(Queued), m_reply(nullptr), m_manager(new QNetworkAccessManager(this)), m_transferRate(0),
    m_speedLimit(0), m_bytesReadThisSecond(0), m_numChunks(1), m_supportsRange(true), m_file(nullptr),
    m_isSingleChunk(false), m_lastUpdateTime(QDateTime::currentMSecsSinceEpoch())
{
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
 * Stops any ongoing download and ensures cleanup by deleting the network manager.
 */

DownloadItem::~DownloadItem()
{
    stop();
    delete m_manager; // Direct deletion to ensure cleanup
}

/**
 * @brief Creates a QNetworkRequest object with custom attributes set.
 *
 * @param url The URL for the request.
 * @return A QNetworkRequest object with HTTP pipelining enabled, cache control set to always use network, and a user agent string set to a modern browser.
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
 * @brief Sets the state of the DownloadItem and emits a stateChanged signal
 *        if the state changes.
 *
 * @param state The new state of the DownloadItem.
 *
 * @note If the state does not change, no signal is emitted.
 */
void DownloadItem::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

/**
 * @brief Sets the network proxy for the DownloadItem.
 *
 * @param proxy The QNetworkProxy object to be used for network requests.
 *
 * @details This function assigns the specified proxy to the network 
 * manager used by the DownloadItem if the network manager is initialized.
 */

void DownloadItem::setProxy(const QNetworkProxy &proxy)
{
    if (m_manager) m_manager->setProxy(proxy);
}


/**
 * @brief Sets the speed limit of the DownloadItem in bytes per second.
 *
 * @details The speed limit is enforced by pausing the download thread if the
 *          limit is exceeded. The speed limit is reset every second.
 *
 * @param bytesPerSec The speed limit to set in bytes per second.
 */
void DownloadItem::setSpeedLimit(qint64 bytesPerSec)
{
    QMutexLocker locker(&m_speedLimitMutex);
    m_speedLimit = bytesPerSec;
    m_bytesReadThisSecond = 0;
    m_speedLimitTimer.restart();
}

void DownloadItem::enforceSpeedLimit(qint64 bytesToRead)
{
    if (m_speedLimit <= 0) return;

    QMutexLocker locker(&m_speedLimitMutex);
    qint64 elapsed = m_speedLimitTimer.elapsed();
    if (elapsed >= 1000) {
        m_bytesReadThisSecond = 0;
        m_speedLimitTimer.restart();
    } else if (m_bytesReadThisSecond + bytesToRead > m_speedLimit) {
        qint64 excess = m_bytesReadThisSecond + bytesToRead - m_speedLimit;
        qint64 sleepTime = (excess * 1000) / m_speedLimit;
        if (sleepTime > 0) QThread::msleep(sleepTime);
        m_bytesReadThisSecond = 0;
        m_speedLimitTimer.restart();
    }
    m_bytesReadThisSecond += bytesToRead;
}

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
    if (!dir.exists() && !dir.mkpath(".")) {
        setState(Failed);
        emit failed("Could not create download directory");
        return;
    }

    setState(Downloading);
    setLastTryDate(QDateTime::currentDateTime());
    fetchTotalSize();
}

void DownloadItem::startSingleChunkDownload()
{
    if (!m_url.isValid() || m_state != Downloading) return;

    m_file = new QFile(m_fullFilePath);
    if (!m_file || (!m_file->open(QFile::exists(m_fullFilePath) ? QIODevice::Append : QIODevice::WriteOnly))) {
        delete m_file;
        m_file = nullptr;
        setState(Failed);
        emit failed("Could not open output file");
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

    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadItem::onSingleChunkReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadItem::onSingleChunkFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadItem::onError);
}

void DownloadItem::onSingleChunkReadyRead()
{
    if (!m_reply || !m_file || !m_file->isOpen()) return;

    QByteArray data = m_reply->readAll();
    if (data.isEmpty()) return;

    enforceSpeedLimit(data.size());
    qint64 bytesWritten = m_file->write(data);
    if (bytesWritten > 0) {
        m_downloadedSize += bytesWritten;
        if (m_totalSize <= 0) m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        emit progress(m_downloadedSize, m_totalSize > 0 ? m_totalSize : m_downloadedSize);
    }
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
 *
 * This function is called when the HEAD request finishes, whether it is successful
 * or encounters an error. It attempts to determine the total size of the file to be
 * downloaded and checks if the server supports range requests (necessary for chunked downloads).
 * If the HEAD request is successful, it sets the total size and the chunking strategy.
 * If the HEAD request fails, it defaults to a single chunk download and attempts to
 * retrieve the file size using a GET request as a fallback.
 */

void DownloadItem::onHeadFinished()
{
    if (!m_reply) return;

    if (m_reply->error() == QNetworkReply::NoError) {
        m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        m_supportsRange = m_reply->rawHeader("Accept-Ranges").toLower().contains("bytes");
        m_isSingleChunk = !m_supportsRange || m_totalSize <= 0;
        m_numChunks = m_isSingleChunk ? 1 : qBound(4, (int)(m_totalSize / (5 * 1024 * 1024)), 16);
    } else {
        m_isSingleChunk = true;
        m_numChunks = 1;
        fetchSizeWithGet(); // Fallback to GET if HEAD fails
    }

    m_reply->deleteLater();
    m_reply = nullptr;
    if (m_state != Failed && !m_isSingleChunk) startChunkDownloads();
}

/**
 * Initiates a GET request to estimate the file size as a fallback.
 *
 * This method is called when the HEAD request fails. It attempts to
 * determine the total size of the file by issuing a GET request. If
 * the URL is invalid, the download is marked as failed. Upon successful
 * initiation of the GET request, it connects the necessary signals to
 * handle ready read, finished, and error events.
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

void DownloadItem::onSingleChunkFinished()
{
    if (!m_reply) return;

    if (m_file && m_file->isOpen()) m_file->close();
    if (m_reply->error() == QNetworkReply::NoError) {
        if (m_totalSize <= 0) m_totalSize = m_downloadedSize;
        setState(Completed);
        emit finished();
    } else if (m_reply->error() != QNetworkReply::OperationCanceledError) {
        setState(Failed);
        emit failed(m_reply->errorString());
    }

    m_reply->deleteLater();
    m_reply = nullptr;
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

    QByteArray data = m_chunkReplies[chunkIndex]->readAll();
    if (data.isEmpty()) return;

    enforceSpeedLimit(data.size());
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

qint64 DownloadItem::getChunkProgress(int chunkIndex) const
{
    return chunkIndex >= 0 && chunkIndex < m_numChunks ? m_chunkDownloaded[chunkIndex] : 0;
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
}
