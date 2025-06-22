#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

private slots:
    void closeEvent(QCloseEvent *event) override;
    void closeDialog();

private:
    Ui::AboutDialog *ui;
    QString abt_dlg;
};


#endif // ABOUTDIALOG_H
