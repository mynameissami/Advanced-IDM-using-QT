#include "newdownloaddialog.h"
#include "ui_newdownloaddialog.h"
#include <QFileDialog>
#include <QStandardPaths>

NewDownloadDialog::NewDownloadDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewDownloadDialog)
{
    ui->setupUi(this);
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime().addSecs(60)); // Default to 1 minute from now
    ui->dateTimeEdit->setEnabled(false);
    ui->savePathLineEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    m_savePath = ui->savePathLineEdit->text();
}

NewDownloadDialog::~NewDownloadDialog()
{
    delete ui;
}

QUrl NewDownloadDialog::getUrl() const
{
    return QUrl::fromUserInput(ui->urlLineEdit->text());
}

bool NewDownloadDialog::isScheduled() const
{
    return ui->scheduleCheckBox->isChecked();
}

QDateTime NewDownloadDialog::getScheduledTime() const
{
    return ui->dateTimeEdit->dateTime();
}

QString NewDownloadDialog::getSavePath() const
{
    return m_savePath;
}

void NewDownloadDialog::on_scheduleCheckBox_toggled(bool checked)
{
    ui->dateTimeEdit->setEnabled(checked);
}

void NewDownloadDialog::on_browseButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Download Folder"),
                                                    m_savePath,
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        m_savePath = dir;
        ui->savePathLineEdit->setText(m_savePath);
    }
}