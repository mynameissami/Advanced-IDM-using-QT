#include "downloadconfirmationdialog.h"
#include "ui_downloadconfirmationdialog.h"
#include <QFileInfo>

DownloadConfirmationDialog::DownloadConfirmationDialog(const QUrl &url, const QStringList &categories, QWidget *parent)
    : QDialog(parent), ui(new Ui::DownloadConfirmationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Confirm Download"));

    ui->urlLabel->setText(url.toString());
    ui->categoryComboBox->addItems(categories);
    ui->categoryComboBox->setCurrentText("All Downloads");
    ui->fileNameLineEdit->setText(QFileInfo(url.path()).fileName().isEmpty() ? "download" : QFileInfo(url.path()).fileName());
    ui->youtubeCheckBox->setVisible(url.toString().contains("youtube.com") || url.toString().contains("youtu.be"));
    ui->startTimeEdit->setDateTime(QDateTime::currentDateTime().addSecs(300)); // 5 minutes from now

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DownloadConfirmationDialog::on_buttonBox_accepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &DownloadConfirmationDialog::on_buttonBox_rejected);
}

DownloadConfirmationDialog::~DownloadConfirmationDialog()
{
    delete ui;
}

void DownloadConfirmationDialog::on_buttonBox_accepted()
{
    m_confirmed = true;
    accept();
}

void DownloadConfirmationDialog::on_buttonBox_rejected()
{
    m_confirmed = false;
    reject();
}

QString DownloadConfirmationDialog::getSelectedCategory() const
{
    return ui->categoryComboBox->currentText();
}

QString DownloadConfirmationDialog::getCustomFileName() const
{
    return ui->fileNameLineEdit->text();
}

bool DownloadConfirmationDialog::shouldShowYouTubeDialog() const
{
    return ui->youtubeCheckBox->isChecked();
}

int DownloadConfirmationDialog::getSpeedLimit() const
{
    return ui->speedLimitSpinBox->value();
}

QDateTime DownloadConfirmationDialog::getScheduledStart() const
{
    return ui->startTimeEdit->dateTime();
}

bool DownloadConfirmationDialog::isHighPriority() const
{
    return ui->priorityCheckBox->isChecked();
}
