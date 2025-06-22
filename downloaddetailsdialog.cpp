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
        qDebug() << "Warning: DownloadItem is null in DownloadDetailsDialog";
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

    // Setup timer for chunk progress updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DownloadDetailsDialog::updateDetails);
    m_updateTimer->start(1000); // Update every second

    // Initial chunk setup
    int numChunks = item->getNumChunks();
    ui->chunkLayout->setAlignment(Qt::AlignTop);
    for (int i = 0; i < numChunks; ++i) {
        QProgressBar *progressBar = new QProgressBar(ui->chunkScrollArea);
        progressBar->setMinimumHeight(30);
        progressBar->setMinimumWidth(200);
        QLabel *chunkLabel = new QLabel(QString("Chunk %1").arg(i), ui->chunkScrollArea);
        ui->chunkLayout->addRow(chunkLabel, progressBar);
        progressBar->setRange(0, item->getTotalSize() / numChunks > 0 ? item->getTotalSize() / numChunks : 100);
        progressBar->setValue(item->getChunkProgress(i));
        if (item->getTotalSize() / numChunks > 0) {
            int percentage = (item->getChunkProgress(i) * 100) / (item->getTotalSize() / numChunks);
            progressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(item->getChunkProgress(i))).arg(formatSize(item->getTotalSize() / numChunks)).arg(percentage));
        } else {
            progressBar->setFormat(QString("%1/N/A").arg(formatSize(item->getChunkProgress(i))));
        }
    }

    // Connect signals
    connect(item, &DownloadItem::progress, this, &DownloadDetailsDialog::onProgress);
    connect(item, &DownloadItem::stateChanged, this, &DownloadDetailsDialog::onStateChanged);
}

DownloadDetailsDialog::~DownloadDetailsDialog()
{
    if (m_updateTimer) m_updateTimer->stop();
    delete ui;
}

void DownloadDetailsDialog::updateDetails()
{
    if (!m_item) return;

    ui->downloadedSizeValueLabel->setText(formatSize(m_item->getDownloadedSize()));
    ui->transferRateValueLabel->setText(m_item->getTransferRate() > 0 ? QString("%1/s").arg(formatSize(m_item->getTransferRate())) : "N/A");

    int numChunks = m_item->getNumChunks();
    for (int i = 0; i < numChunks; ++i) {
        QLayoutItem *item = ui->chunkLayout->itemAt(i * 2 + 1); // Skip labels, get progress bars
        if (item && item->widget()) {
            QProgressBar *progressBar = qobject_cast<QProgressBar*>(item->widget());
            if (progressBar) {
                progressBar->setValue(m_item->getChunkProgress(i));
                if (m_item->getTotalSize() / numChunks > 0) {
                    int percentage = (m_item->getChunkProgress(i) * 100) / (m_item->getTotalSize() / numChunks);
                    progressBar->setFormat(QString("%1/%2 (%3%)").arg(formatSize(m_item->getChunkProgress(i))).arg(formatSize(m_item->getTotalSize() / numChunks)).arg(percentage));
                } else {
                    progressBar->setFormat(QString("%1/N/A").arg(formatSize(m_item->getChunkProgress(i))));
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
    updateDetails(); // Sync chunk progress
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
