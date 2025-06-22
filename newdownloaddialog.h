#ifndef NEWDOWNLOADDIALOG_H
#define NEWDOWNLOADDIALOG_H

#include <QDialog>
#include <QUrl>
#include <QDateTime>

namespace Ui {
class NewDownloadDialog;
}

class NewDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewDownloadDialog(QWidget *parent = nullptr);
    ~NewDownloadDialog();

    QUrl getUrl() const;
    bool isScheduled() const;
    QDateTime getScheduledTime() const;
    QString getSavePath() const;

private slots:
    void on_scheduleCheckBox_toggled(bool checked);
    void on_browseButton_clicked();

private:
    Ui::NewDownloadDialog *ui;
    QString m_savePath;
};

#endif // NEWDOWNLOADDIALOG_H