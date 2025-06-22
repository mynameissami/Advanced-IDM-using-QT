#include "connectionsettingsdialog.h"
#include "ui_connectionsettingsdialog.h"

ConnectionSettingsDialog::ConnectionSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionSettingsDialog)
{
    ui->setupUi(this);

    ui->proxyTypeCombo->addItem("No Proxy", QNetworkProxy::NoProxy);
    ui->proxyTypeCombo->addItem("HTTP Proxy", QNetworkProxy::HttpProxy);
    ui->proxyTypeCombo->addItem("Socks5 Proxy", QNetworkProxy::Socks5Proxy);
}

ConnectionSettingsDialog::~ConnectionSettingsDialog()
{
    delete ui;
}

QNetworkProxy ConnectionSettingsDialog::proxy() const
{
    QNetworkProxy proxy;
    proxy.setType(static_cast<QNetworkProxy::ProxyType>(ui->proxyTypeCombo->currentData().toInt()));
    proxy.setHostName(ui->hostEdit->text());
    proxy.setPort(ui->portSpin->value());
    proxy.setUser(ui->usernameEdit->text());
    proxy.setPassword(ui->passwordEdit->text());
    return proxy;
}

void ConnectionSettingsDialog::setProxy(const QNetworkProxy &proxy)
{
    int index = ui->proxyTypeCombo->findData(proxy.type());
    if (index != -1)
        ui->proxyTypeCombo->setCurrentIndex(index);

    ui->hostEdit->setText(proxy.hostName());
    ui->portSpin->setValue(proxy.port());
    ui->usernameEdit->setText(proxy.user());
    ui->passwordEdit->setText(proxy.password());
}
