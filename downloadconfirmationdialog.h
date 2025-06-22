#ifndef DOWNLOADCONFIRMATIONDIALOG_H
#define DOWNLOADCONFIRMATIONDIALOG_H

#include <QDialog>
#include <QUrl>

namespace Ui {
class DownloadConfirmationDialog;
}

class DownloadConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadConfirmationDialog(const QUrl &url, const QStringList &categories, QWidget *parent = nullptr);
    ~DownloadConfirmationDialog();

    bool isConfirmed() const { return m_confirmed; }
    QString getSelectedCategory() const;
    QString getCustomFileName() const;
    bool shouldShowYouTubeDialog() const;
    int getSpeedLimit() const;
    QDateTime getScheduledStart() const;
    bool isHighPriority() const;

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::DownloadConfirmationDialog *ui;
    bool m_confirmed = false;
};

#endif // DOWNLOADCONFIRMATIONDIALOG_H
