#ifndef SPEEDLIMITERDIALOG_H
#define SPEEDLIMITERDIALOG_H

#include <QDialog>

namespace Ui {
class SpeedLimiterDialog;
}

class SpeedLimiterDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpeedLimiterDialog(QWidget *parent = nullptr);
    ~SpeedLimiterDialog();

    // Getter methods
    int getSpeedLimitKBps() const;
    bool isSpeedLimitEnabled() const;

    // Setter methods
    void setSpeedLimitEnabled(bool enabled);
    void setSpeedLimitKBps(int kbPerSec);

private:
    Ui::SpeedLimiterDialog *ui;
};

#endif // SPEEDLIMITERDIALOG_H
