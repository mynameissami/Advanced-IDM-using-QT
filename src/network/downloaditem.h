#ifndef DOWNLOADITEM_H
#define DOWNLOADITEM_H

#include <QObject>
#include <QUrl>
#include <QNetworkReply>
#include <QFile>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkProxy>
#include <QMutex>
#include <QThread>
#include <QElapsedTimer>

// Forward declaration
class SpeedLimitWorker;

class DownloadItem : public QObject
{
    Q_OBJECT
public:
    enum State { Queued, Downloading, Paused, Stopped, Completed, Failed };
    Q_ENUM(State)

    explicit DownloadItem(const QUrl &url, const QString &filePath, QObject *parent = nullptr);
    ~DownloadItem();
    // --- Core Public API ---
    void start();
    void pause();
    void resume();
    void stop();

    // --- Setters ---
    void setState(State state);
    void setProxy(const QNetworkProxy &proxy);
    void setSpeedLimit(qint64 bytesPerSec);
    void setFileName(const QString &name) { m_fileName = name; }
    void setNumChunks(int num) { m_numChunks = num; }
    void setUrl(const QUrl &url) { m_url = url; }
    void setLastTryDate(const QDateTime &date) { m_lastTryDate = date; }
    void setDescription(const QString &desc) { m_description = desc; }
    void setTotalSize(qint64 size) { m_totalSize = size; }
    void setDownloadedSize(qint64 size) { m_downloadedSize = size; }
    void setFullFilePath(const QString &path);

    // --- Getters ---
    State getState() const { return m_state; }
    QUrl getUrl() const { return m_url; }
    QString getFileName() const { return m_fileName; }
    QString getFullFilePath() const { return m_fullFilePath; }
    qint64 getTotalSize() const { return m_totalSize; }
    qint64 getDownloadedSize() const { return m_downloadedSize; }
    qint64 getTransferRate() const { return m_transferRate; }
    qint64 getCurrentSpeedLimit();
    QDateTime getLastTryDate() const { return m_lastTryDate; }
    qint64 getChunkProgress(int chunkIndex) const;
    QString getDescription() const { return m_description; }
    int getNumChunks() const { return m_numChunks; }
    bool isSingleChunk() const { return m_isSingleChunk; }

    // Friend declaration to allow SpeedLimitWorker access to private members
    friend class SpeedLimitWorker;

signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();
    void failed(const QString &reason);
    void stateChanged(State state);

private slots:
    void onHeadFinished();
    void onChunkReadyRead(int chunkIndex);
    void onChunkFinished(int chunkIndex);
    void onError(QNetworkReply::NetworkError code);
    void updateTransferRate();
    void fetchSizeWithGet();
    void onGetReadyRead();
    void onGetFinished();

private:
    void enforceSpeedLimit(qint64 bytesToRead);
    void fetchTotalSize();
    void startChunkDownloads();
    void cleanup(bool deleteFiles);
    bool checkPartialChunks();
    void initializeChunks();
    void mergeChunks();
    void startOrResumeChunk(int chunkIndex);
    void startSingleChunkDownload();
    void onSingleChunkReadyRead();
    void onSingleChunkFinished();

    QNetworkRequest createNetworkRequest(const QUrl &url);
    qint64 m_lastUpdateTime;
    QUrl m_url;
    QFile *m_file;
    QString m_fileName;
    QString m_fullFilePath;
    qint64 m_totalSize;
    qint64 m_downloadedSize;
    State m_state;

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;

    int m_numChunks;
    bool m_supportsRange;
    bool m_isSingleChunk;
    QList<qint64> m_chunks;
    QList<QNetworkReply*> m_chunkReplies;
    QList<QFile*> m_chunkFiles;
    QList<qint64> m_chunkDownloaded;

    QTimer *m_rateTimer;
    qint64 m_bytesLastPeriod;
    qint64 m_transferRate;

    qint64 m_speedLimit = 0;
    qint64 m_bytesReadThisSecond = 0;
    QElapsedTimer m_speedLimitTimer;
    QMutex m_speedLimitMutex;
    SpeedLimitWorker* m_worker = nullptr;
    QThread* m_workerThread = nullptr;
    QDateTime m_lastTryDate;
    QString m_description;
    QMutex m_chunkMutex;
    bool validateChunk(int chunkIndex);
};

// Definition of SpeedLimitWorker outside DownloadItem
class SpeedLimitWorker : public QObject {
    Q_OBJECT
public:
    explicit SpeedLimitWorker(DownloadItem* parent);
    ~SpeedLimitWorker();

public slots:
    void enforceLimit(qint64 bytesToRead);

private:
    DownloadItem* m_parent;
    QElapsedTimer* m_timer;
};

#endif // DOWNLOADITEM_H
