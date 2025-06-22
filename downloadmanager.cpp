#include "downloadmanager.h"
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QDir>
#include <QTime>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent), m_maxConcurrentDownloads(3), m_globalSpeedLimit(0), m_speedLimitEnabled(false)
{
}

DownloadManager::~DownloadManager()
{
    stopAll();
    qDeleteAll(m_downloadQueue);
    m_downloadQueue.clear();
}

void DownloadManager::addToQueue(DownloadItem *item)
{
    if (!item || m_downloadQueue.contains(item) || m_activeDownloads.contains(item)) return;

    m_downloadQueue.append(item);
    emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
    startNextInQueue();
}

void DownloadManager::startNextInQueue()
{
    while (m_activeDownloads.size() < m_maxConcurrentDownloads && !m_downloadQueue.isEmpty()) {
        DownloadItem *item = m_downloadQueue.takeFirst();
        if (item) {
            m_activeDownloads.append(item);
            connect(item, &DownloadItem::finished, this, &DownloadManager::handleItemFinishedOrFailed);
            connect(item, &DownloadItem::failed, this, &DownloadManager::handleItemFinishedOrFailed);
            applySettingsToItem(item);
            item->start();
        }
    }
    emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
}

void DownloadManager::pauseAll()
{
    qDebug() << "Pausing all downloads. Active count:" << m_activeDownloads.size();
    for (DownloadItem *item : m_activeDownloads) {
        if (item && item->getState() == DownloadItem::Downloading) {
            qDebug() << "Pausing item:" << item->getFileName();
            item->pause();
            m_activeDownloads.removeOne(item);
            m_downloadQueue.prepend(item);
        }
    }
    emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
}
void DownloadManager::resumeAll()
{
    startNextInQueue();
}

void DownloadManager::stopAll()
{
    for (DownloadItem *item : m_activeDownloads) item->stop();
    m_activeDownloads.clear();
    for (DownloadItem *item : m_downloadQueue) item->stop();
    m_downloadQueue.clear();
    emit queueStatusChanged(0, 0);
}

void DownloadManager::handleItemFinishedOrFailed()
{
    DownloadItem *item = qobject_cast<DownloadItem*>(sender());
    if (item) {
        disconnect(item, &DownloadItem::finished, this, &DownloadManager::handleItemFinishedOrFailed);
        disconnect(item, &DownloadItem::failed, this, &DownloadManager::handleItemFinishedOrFailed);
        m_activeDownloads.removeOne(item);

        if (item->getState() != DownloadItem::Completed) m_downloadQueue.prepend(item);
        startNextInQueue();
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
    }
}

void DownloadManager::applySettingsToItem(DownloadItem *item)
{
    if (item) {
        item->setProxy(m_proxy);
        item->setSpeedLimit(m_speedLimitEnabled ? m_globalSpeedLimit : 0);
    }
}

void DownloadManager::setMaxConcurrentDownloads(int max)
{
    m_maxConcurrentDownloads = qMax(1, max);
    startNextInQueue();
}

void DownloadManager::downloadYouTube(DownloadItem *item)
{
    if (!item) return;

    QString url = item->getUrl().toString();
    QString outputPath = item->getFullFilePath();
    QProcess *process = new QProcess(this);
    QStringList args = {"-o", outputPath, "--merge-output-format", "mp4", "--restrict-filenames", url};

    connect(process, &QProcess::readyReadStandardOutput, [item, process, this]() {
        QString output = process->readAllStandardOutput();
        QRegularExpression progressRegex("(\\d+\\.\\d+)% of (\\d+\\.\\d+)([KMG])iB");
        QRegularExpressionMatch match = progressRegex.match(output);
        if (match.hasMatch()) {
            qreal percent = match.captured(1).toDouble();
            QString sizeStr = match.captured(2) + match.captured(3);
            bool ok;
            qreal sizeValue = sizeStr.left(sizeStr.size() - 2).toDouble(&ok);
            if (ok) {
                qreal multiplier = (match.captured(3) == "G") ? 1e9 : (match.captured(3) == "M") ? 1e6 : 1e3;
                qint64 totalSize = static_cast<qint64>(sizeValue * multiplier);
                qint64 downloadedSize = static_cast<qint64>((percent / 100.0) * totalSize);
                item->setTotalSize(totalSize);
                item->setDownloadedSize(downloadedSize);
                emit item->progress(downloadedSize, totalSize);
            }
        }
    });

    connect(process, &QProcess::finished, [this, item, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            item->setState(DownloadItem::Completed);
            emit item->finished();
        } else {
            item->setState(DownloadItem::Failed);
            emit item->failed(process->readAllStandardError());
        }
        process->deleteLater();
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
    });

    if (QProcess::execute("yt-dlp", {"--version"}) != 0) {
        item->setState(DownloadItem::Failed);
        emit item->failed("yt-dlp is not installed or not found in PATH. Please install it.");
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
        return;
    }

    item->setState(DownloadItem::Downloading);
    process->start("yt-dlp", args);
    m_activeDownloads.append(item);
    emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
}

void DownloadManager::setProxy(const QNetworkProxy &proxy)
{
    m_proxy = proxy;
    for (DownloadItem *item : m_activeDownloads) applySettingsToItem(item);
}

void DownloadManager::setGlobalSpeedLimit(qint64 bytesPerSec, bool enabled)
{
    m_globalSpeedLimit = bytesPerSec;
    m_speedLimitEnabled = enabled;
    for (DownloadItem *item : m_activeDownloads) applySettingsToItem(item);
}

bool DownloadManager::isItemActive(DownloadItem *item) const
{
    return m_activeDownloads.contains(item);
}

int DownloadManager::getQueuePosition(DownloadItem *item) const
{
    return m_downloadQueue.indexOf(item);
}

void DownloadManager::processQueue()
{
    startNextInQueue();
}

void DownloadManager::downloadYouTubeWithOptions(DownloadItem *item, const QStringList &args)
{
    if (!item) return;

    QString url = item->getUrl().toString();
    QString outputPath = item->getFullFilePath(); // Should be set from dialog
    QDir dir(QFileInfo(outputPath).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        item->setState(DownloadItem::Failed);
        emit item->failed(tr("Cannot create directory for output file: %1").arg(outputPath));
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
        return;
    }

    QProcess *process = new QProcess(this);
    QStringList fullArgs = args; // Copy the input args
    fullArgs << "-o" << outputPath << url; // Append items correctly

    connect(process, &QProcess::readyReadStandardOutput, [item, process, this]() {
        QString output = process->readAllStandardOutput();
        QRegularExpression progressRegex("(\\d+\\.\\d+)% of (\\d+\\.\\d+)([KMG])iB");
        QRegularExpressionMatch match = progressRegex.match(output);
        if (match.hasMatch()) {
            qreal percent = match.captured(1).toDouble();
            QString sizeStr = match.captured(2) + match.captured(3);
            bool ok;
            qreal sizeValue = sizeStr.left(sizeStr.size() - 2).toDouble(&ok);
            if (ok) {
                qreal multiplier = (match.captured(3) == "G") ? 1e9 : (match.captured(3) == "M") ? 1e6 : 1e3;
                qint64 totalSize = static_cast<qint64>(sizeValue * multiplier);
                qint64 downloadedSize = static_cast<qint64>((percent / 100.0) * totalSize);
                item->setTotalSize(totalSize);
                item->setDownloadedSize(downloadedSize);
                emit item->progress(downloadedSize, totalSize);
            }
        }
    });

    connect(process, &QProcess::finished, [this, item, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            item->setState(DownloadItem::Completed);
            emit item->finished();
        } else {
            item->setState(DownloadItem::Failed);
            emit item->failed(tr("yt-dlp failed: %1").arg(process->readAllStandardError()));
        }
        process->deleteLater();
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
    });

    if (QProcess::execute("yt-dlp", {"--version"}) != 0) {
        item->setState(DownloadItem::Failed);
        emit item->failed("yt-dlp is not installed or not found in PATH. Please install it.");
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
        return;
    }

    item->setState(DownloadItem::Downloading);
    process->start("yt-dlp", fullArgs);
    m_activeDownloads.append(item);
    emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
}
