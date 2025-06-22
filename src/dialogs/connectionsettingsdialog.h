#ifndef CONNECTIONSETTINGSDIALOG_H
#define CONNECTIONSETTINGSDIALOG_H

#include <QDialog>
#include <QNetworkProxy>

namespace Ui {
class ConnectionSettingsDialog;
}

class ConnectionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionSettingsDialog(QWidget *parent = nullptr);
    ~ConnectionSettingsDialog();

    void setProxy(const QNetworkProxy &proxy);
    QNetworkProxy getProxy() const;
    QNetworkProxy proxy() const;

private:
    Ui::ConnectionSettingsDialog *ui;

};

#endif // CONNECTIONSETTINGSDIALOG_H