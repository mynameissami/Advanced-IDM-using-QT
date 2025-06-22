#include "preferencesdialog.h"
#include "ui_PreferencesDialog.h"

#include <QFileDialog>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    // Browse button to select download folder
    connect(ui->browseButton, &QPushButton::clicked, this, [=]() {
        QString folder = QFileDialog::getExistingDirectory(this, "Select Download Folder");
        if (!folder.isEmpty()) {
            ui->downloadFolderLineEdit->setText(folder);
        }
    });

    // OK button: accept dialog (save settings)
    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);

    // Cancel button: reject dialog (discard)
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}
PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

QString PreferencesDialog::getDefaultFolder() const {
    return ui->downloadFolderLineEdit->text();
}

bool PreferencesDialog::isAutoStartEnabled() const {
    return ui->autoStartCheckBox->isChecked();
}

bool PreferencesDialog::shouldPromptBeforeOverwrite() const {
    return ui->promptOverwriteCheckBox->isChecked();
}

QString PreferencesDialog::getFileNamingPolicy() const {
    return ui->namingPolicyComboBox->currentText();
}

int PreferencesDialog::getMaxConcurrentDownloads() const {
    return ui->maxDownloadsSpinBox->value();
}

void PreferencesDialog::setDefaultFolder(const QString &folder) {
    ui->downloadFolderLineEdit->setText(folder);
}

void PreferencesDialog::setAutoStart(bool enabled) {
    ui->autoStartCheckBox->setChecked(enabled);
}

void PreferencesDialog::setPromptBeforeOverwrite(bool enabled) {
    ui->promptOverwriteCheckBox->setChecked(enabled);
}

void PreferencesDialog::setFileNamingPolicy(const QString &policy) {
    int index = ui->namingPolicyComboBox->findText(policy);
    if (index >= 0)
        ui->namingPolicyComboBox->setCurrentIndex(index);
}

void PreferencesDialog::setMaxConcurrentDownloads(int max) {
    ui->maxDownloadsSpinBox->setValue(max);
}

void PreferencesDialog::on_browseButton_clicked() {
    QString folder = QFileDialog::getExistingDirectory(this, "Select Default Folder");
    if (!folder.isEmpty())
        ui->downloadFolderLineEdit->setText(folder);
}
