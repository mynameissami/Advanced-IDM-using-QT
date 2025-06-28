#include "downloaddetailsdialog.h"
#include "ui_downloaddetailsdialog.h"
#include <QTimer>
#include <QProgressBar>
#include <QLabel>

DownloadDetailsDialog::DownloadDetailsDialog(DownloadItem *item, QWidget *parent)
    : QDialog(parent), ui(new Ui::DownloadDetailsDialog), m_item(item)
{
    ui->setupUi(this);
    if (!item) {
        qDebug() << "Warning: DownloadItem is null in DownloadDetailsDialog at" << QDateTime::currentDateTime().toString("hh:mm:ss");
        return;
    }

    setWindowTitle(tr("Download Details - %1").arg(item->getFileName()));

    // Initial setup
    ui->urlValueLabel->setText(item->getUrl().toString());
    ui->filePathValueLabel->setText(item->getFullFilePath());
    ui->statusValueLabel->setText(item->getState() == DownloadItem::Downloading ? "Downloading" :
                                      item->getState() == DownloadItem::Completed ? "Completed" :
                                      item->getState() == DownloadItem::Failed ? "Failed" : "Queued/Paused");
    ui->totalSizeValueLabel->setText(item->getTotalSize() >= 0 ? formatSize(item->getTotalSize()) : "Unknown");
    ui->downloadedSizeValueLabel->setText(formatSize(item->getDownloadedSize()));
    ui->transferRateValueLabel->setText(item->getTransferRate() > 0 ? QString("%1/s").arg(formatSize(item->getTransferRate())) : "N/A");
    ui->lastTryValueLabel->setText(item->getLastTryDate().toString("yyyy-MM-dd hh:mm"));
    ui->descriptionValueLabel->setText(item->getDescription());

    // Setup timer for updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DownloadDetailsDialog::updateDetails);
    m_updateTimer->start(1000); // Update every second

    // Handle chunk or single-chunk display
    int numChunks = item->getNumChunks();
    qDebug() << "DownloadDetailsDialog: Setting up for" << item->getFileName()
             << ", numChunks:" << numChunks
             << ", downloadedSize:" << item->getDownloadedSize()
             << ", totalSize:" << item->getTotalSize()
             << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->chunkLayout->setAlignment(Qt::AlignTop);
    if (numChunks == 1) {
        m_singleChunkProgressBar = new QProgressBar(ui->chunkScrollArea);
        if (!m_singleChunkProgressBar) {
            qDebug() << "DownloadDetailsDialog: Failed to create singleChunkProgressBar for" << item->getFileName()
            << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
            return;
        }
        m_singleChunkProgressBar->setMinimumHeight(30);
        m_singleChunkProgressBar->setMinimumWidth(200);
        QLabel *progressLabel = new QLabel(tr("Overall Progress"), ui->chunkScrollArea);
        if (!progressLabel) {
            qDebug() << "DownloadDetailsDialog: Failed to create progressLabel for" << item->getFileName()
            << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
            delete m_singleChunkProgressBar;
            m_singleChunkProgressBar = nullptr;
            return;
        }
        ui->chunkLayout->addRow(progressLabel, m_singleChunkProgressBar);
        m_singleChunkProgressBar->setRange(0, item->getTotalSize() > 0 ? item->getTotalSize() : 100);
        qint64 downloaded = item->getDownloadedSize();
        m_singleChunkProgressBar->setValue(downloaded);
        if (item->getTotalSize() > 0) {
            int percentage = (downloaded * 100) / item->getTotalSize();
            m_singleChunkProgressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(downloaded)).arg(formatSize(item->getTotalSize())).arg(percentage));
        } else {
            m_singleChunkProgressBar->setFormat(QString("%1/N/A").arg(formatSize(downloaded)));
        }
    } else if (numChunks > 1) {
        for (int i = 0; i < numChunks; ++i) {
            QProgressBar *progressBar = new QProgressBar(ui->chunkScrollArea);
            if (!progressBar) {
                qDebug() << "DownloadDetailsDialog: Failed to create progressBar for chunk" << i << "of" << item->getFileName()
                << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
                for (int j = 0; j < i; ++j) {
                    QLayoutItem *layoutItem = ui->chunkLayout->itemAt(j * 2 + 1);
                    if (layoutItem && layoutItem->widget()) delete layoutItem->widget();
                }
                return;
            }
            progressBar->setMinimumHeight(30);
            progressBar->setMinimumWidth(200);
            QLabel *chunkLabel = new QLabel(QString("Chunk %1").arg(i), ui->chunkScrollArea);
            if (!chunkLabel) {
                qDebug() << "DownloadDetailsDialog: Failed to create chunkLabel for chunk" << i << "of" << item->getFileName()
                << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
                delete progressBar;
                for (int j = 0; j < i; ++j) {
                    QLayoutItem *layoutItem = ui->chunkLayout->itemAt(j * 2 + 1);
                    if (layoutItem && layoutItem->widget()) delete layoutItem->widget();
                }
                return;
            }
            ui->chunkLayout->addRow(chunkLabel, progressBar);
            qint64 chunkTotal = item->getTotalSize() / numChunks;
            progressBar->setRange(0, chunkTotal > 0 ? chunkTotal : 100);
            qint64 chunkProgress = item->getChunkProgress(i);
            progressBar->setValue(chunkProgress);
            if (chunkTotal > 0) {
                int percentage = (chunkProgress * 100) / chunkTotal;
                progressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(chunkProgress)).arg(formatSize(chunkTotal)).arg(percentage));
            } else {
                progressBar->setFormat(QString("%1/N/A").arg(formatSize(chunkProgress)));
            }
        }
    } else {
        qDebug() << "DownloadDetailsDialog: Invalid numChunks:" << numChunks << "for" << item->getFileName()
        << "at" << QDateTime::currentDateTime().toString("hh:mm:ss");
        return;
    }

    connect(item, &DownloadItem::progress, this, &DownloadDetailsDialog::onProgress);
    connect(item, &DownloadItem::stateChanged, this, &DownloadDetailsDialog::onStateChanged);
}

DownloadDetailsDialog::~DownloadDetailsDialog()
{
    if (m_updateTimer) m_updateTimer->stop();
    // Clean up progress bars
    int numItems = ui->chunkLayout->count();
    for (int i = 0; i < numItems; i += 2) {
        QLayoutItem *item = ui->chunkLayout->itemAt(i + 1); // Get progress bar
        if (item && item->widget()) {
            delete item->widget();
        }
    }
    delete ui;
}

void DownloadDetailsDialog::updateDetails()
{
    if (!m_item) return;

    ui->downloadedSizeValueLabel->setText(formatSize(m_item->getDownloadedSize()));
    ui->transferRateValueLabel->setText(m_item->getTransferRate() > 0 ? QString("%1/s").arg(formatSize(m_item->getTransferRate())) : "N/A");

    int numChunks = m_item->getNumChunks();
    if (numChunks == 1 && m_singleChunkProgressBar) {
        qint64 downloaded = m_item->getDownloadedSize();
        m_singleChunkProgressBar->setValue(downloaded);
        if (m_item->getTotalSize() > 0) {
            int percentage = (downloaded * 100) / m_item->getTotalSize();
            m_singleChunkProgressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(downloaded)).arg(formatSize(m_item->getTotalSize())).arg(percentage));
        } else {
            m_singleChunkProgressBar->setFormat(QString("%1/N/A").arg(formatSize(downloaded)));
        }
    } else if (numChunks > 1) {
        for (int i = 0; i < numChunks; ++i) {
            QLayoutItem *item = ui->chunkLayout->itemAt(i * 2 + 1);
            if (item && item->widget()) {
                QProgressBar *progressBar = qobject_cast<QProgressBar*>(item->widget());
                if (progressBar) {
                    qint64 chunkProgress = m_item->getChunkProgress(i);
                    progressBar->setValue(chunkProgress);
                    qint64 chunkTotal = m_item->getTotalSize() / numChunks;
                    if (chunkTotal > 0) {
                        int percentage = (chunkProgress * 100) / chunkTotal;
                        progressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(chunkProgress)).arg(formatSize(chunkTotal)).arg(percentage));
                    } else {
                        progressBar->setFormat(QString("%1/N/A").arg(formatSize(chunkProgress)));
                    }
                }
            }
        }
    }
}

void DownloadDetailsDialog::onProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (!m_item) return;
    ui->downloadedSizeValueLabel->setText(formatSize(bytesReceived));
    if (bytesTotal > 0) ui->totalSizeValueLabel->setText(formatSize(bytesTotal));
    updateDetails();
}

void DownloadDetailsDialog::onStateChanged(DownloadItem::State state)
{
    if (!m_item) return;
    ui->statusValueLabel->setText(state == DownloadItem::Downloading ? "Downloading" :
                                      state == DownloadItem::Completed ? "Completed" :
                                      state == DownloadItem::Failed ? "Failed" : "Queued/Paused");
}

QString DownloadDetailsDialog::formatSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}
