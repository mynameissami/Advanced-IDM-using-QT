#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QNetworkProxy>
#include <QList>
#include "downloaditem.h"

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    // --- Public API ---
    void addToQueue(DownloadItem *item);
    void pauseAll();
    void resumeAll();
    void stopAll();
    void setMaxConcurrentDownloads(int max);
    void setProxy(const QNetworkProxy &proxy);
    bool isItemActive(DownloadItem *item) const;
    int getQueuePosition(DownloadItem *item) const;
    // *** FIX: Re-added for compatibility with MainWindow UI ***
    void setGlobalSpeedLimit(qint64 bytesPerSec, bool enabled);
    void downloadYouTube(DownloadItem *item);
    void downloadYouTubeWithOptions(DownloadItem *item, const QStringList &args);

signals:
    void queueStatusChanged(int activeCount, int queuedCount);

private slots:
    void handleItemFinishedOrFailed();
    void processQueue();

private:
    void startNextInQueue();
    void applySettingsToItem(DownloadItem *item);
    QTimer m_processTimeout;
    QList<DownloadItem*> m_downloadQueue;
    QList<DownloadItem*> m_activeDownloads;
    int m_maxConcurrentDownloads;
    QNetworkProxy m_proxy;
    // Members to store the global speed limit state
    qint64 m_globalSpeedLimit;
    bool m_speedLimitEnabled;

};

#endif // DOWNLOADMANAGER_H
