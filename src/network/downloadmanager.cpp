#include "downloadmanager.h"
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QDir>
#include <QTime>
#include "../utils/utils.h"

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
    for (DownloadItem *item : m_activeDownloads) {
        if (item) { // Guard against null items
            applySettingsToItem(item);
            if (item->getState() == DownloadItem::Downloading) {
                item->setSpeedLimit(m_speedLimitEnabled ? m_globalSpeedLimit : 0);
            }
        }
    }
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
    if (!item) {
        qCritical() << "Invalid DownloadItem in downloadYouTubeWithOptions";
        return;
    }

    QString url = item->getUrl().toString();
    QString outputPath = item->getFullFilePath();
    QProcess *process = new QProcess(this);
    QStringList fullArgs = args;
    fullArgs.removeAll("--restrict-filenames");
    fullArgs << "--verbose" << "-o" << outputPath << url;

    qDebug() << "Starting yt-dlp with args:" << fullArgs << "for URL:" << url << "to" << outputPath;
    connect(process, &QProcess::started, [process]() {
        qDebug() << "yt-dlp process started, PID:" << process->processId();
    });
    connect(process, &QProcess::readyReadStandardOutput, [process, item, this]() {
        QByteArray output = process->readAllStandardOutput();
        qDebug() << "yt-dlp output:" << output;
        // Parse yt-dlp output for progress (e.g., "[download] XX% of YY")
        QRegularExpression progressRegex("\\[download\\]\\s+(\\d+\\.\\d+)% of\\s+(\\d+\\.\\d+[KMG]?B)");
        QRegularExpressionMatch match = progressRegex.match(output);
        if (match.hasMatch()) {
            qreal percent = match.captured(1).toDouble();
            QString totalStr = match.captured(2);
            qint64 totalSize = parseSize(totalStr); // Assume Utils::parseSize converts "1.5MB" to bytes
            qint64 downloadedSize = (percent / 100.0) * totalSize;
            item->setTotalSize(totalSize);
            item->setDownloadedSize(downloadedSize);
            emit item->progress(downloadedSize, totalSize);
        }
    });
    connect(process, &QProcess::readyReadStandardError, [process, item, this]() {
        QByteArray error = process->readAllStandardError();
        qDebug() << "yt-dlp error:" << error;
        if (error.contains("ERROR")) {
            item->setState(DownloadItem::Failed);
            emit item->failed(error);
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, item, process](int exitCode, QProcess::ExitStatus) {
                if (!item || !process) {
                    qCritical() << "Invalid item or process in finished slot";
                    return;
                }
                QByteArray output = process->readAllStandardOutput();
                QByteArray error = process->readAllStandardError();
                qDebug() << "yt-dlp finished with exit code:" << exitCode << "Output:" << output << "Error:" << error;
                if (exitCode == 0) {
                    item->setState(DownloadItem::Completed);
                    emit item->finished();
                } else {
                    qCritical() << "yt-dlp failed with error:" << error;
                    item->setState(DownloadItem::Failed);
                    emit item->failed(tr("yt-dlp failed with exit code %1: %2").arg(exitCode).arg(QString(error)));
                }
                process->deleteLater();
                m_activeDownloads.removeOne(item);
                emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
                startNextInQueue();
            });
    connect(process, &QProcess::errorOccurred, [this, process, item](QProcess::ProcessError error) {
        qCritical() << "yt-dlp process error:" << error << "Details:" << process->errorString()
        << "for" << (item ? item->getUrl().toString() : "null item");
        if (process) process->deleteLater();
        if (item) {
            item->setState(DownloadItem::Failed);
            emit item->failed(tr("Process error: %1").arg(process->errorString()));
        }
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
    });

    if (QProcess::execute("yt-dlp", {"--version"}) != 0) {
        qCritical() << "yt-dlp not found in PATH";
        item->setState(DownloadItem::Failed);
        emit item->failed("yt-dlp is not installed or not found in PATH. Please install it.");
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
        return;
    }

    QFileInfo fileInfo(outputPath);
    if (!fileInfo.dir().exists() || !fileInfo.dir().Writable) {
        qCritical() << "Invalid or unwritable path:" << outputPath;
        item->setState(DownloadItem::Failed);
        emit item->failed("Invalid or unwritable output path.");
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
        return;
    }

    item->setState(DownloadItem::Downloading);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", env.value("PATH") + ";C:\\Python39\\Scripts");
    process->setProcessEnvironment(env);
    if (QProcess::execute("yt-dlp", {"--version"}) != 0) {
        qCritical() << "Failed to start yt-dlp:" << process->errorString();
        item->setState(DownloadItem::Failed);
        emit item->failed(tr("Failed to start process: %1").arg(process->errorString()));
        process->deleteLater();
        m_activeDownloads.removeOne(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
        startNextInQueue();
    } else {
        m_activeDownloads.append(item);
        emit queueStatusChanged(m_activeDownloads.size(), m_downloadQueue.size());
    }
}
