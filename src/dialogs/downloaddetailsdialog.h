#ifndef DOWNLOADDETAILSDIALOG_H
#define DOWNLOADDETAILSDIALOG_H

#include <QDialog>
#include <qprogressbar.h>
#include "../network/downloaditem.h"

namespace Ui {
class DownloadDetailsDialog;
}

class DownloadDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadDetailsDialog(DownloadItem *item, QWidget *parent = nullptr);
    ~DownloadDetailsDialog();

private slots:
    void updateDetails();
    void onProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStateChanged(DownloadItem::State state);

private:
    Ui::DownloadDetailsDialog *ui;
    DownloadItem *m_item;
    QTimer *m_updateTimer;
    QProgressBar *m_singleChunkProgressBar = nullptr; // Added for single-chunk support
    QString formatSize(qint64 bytes) const;
};

#endif // DOWNLOADDETAILSDIALOG_H
