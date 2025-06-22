#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog();

    QString getDefaultFolder() const;
    bool isAutoStartEnabled() const;
    bool shouldPromptBeforeOverwrite() const;
    QString getFileNamingPolicy() const;
    int getMaxConcurrentDownloads() const;

    void setDefaultFolder(const QString &folder);
    void setAutoStart(bool enabled);
    void setPromptBeforeOverwrite(bool enabled);
    void setFileNamingPolicy(const QString &policy);
    void setMaxConcurrentDownloads(int max);

private slots:
    void on_browseButton_clicked();

private:
    Ui::PreferencesDialog *ui;
};

#endif // PREFERENCESDIALOG_H
