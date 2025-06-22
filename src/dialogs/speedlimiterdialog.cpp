#include "speedlimiterdialog.h"
#include "ui_speedlimiterdialog.h"

SpeedLimiterDialog::SpeedLimiterDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SpeedLimiterDialog)
{
    ui->setupUi(this);
    setWindowTitle("Download Speed Limiter");

    // Set reasonable defaults and validation
    ui->speedLimitSpinBox->setRange(10, 100000); // 10 KB/s to 100 MB/s
    ui->speedLimitSpinBox->setValue(100); // Default to 100 KB/s
    ui->speedLimitSpinBox->setSuffix(" KB/s");

    // Connect signals
    connect(ui->enableCheckBox, &QCheckBox::toggled,
            ui->speedLimitSpinBox, &QSpinBox::setEnabled);
}

SpeedLimiterDialog::~SpeedLimiterDialog()
{
    delete ui;
}

int SpeedLimiterDialog::getSpeedLimitKBps() const
{
    return ui->speedLimitSpinBox->value();
}

bool SpeedLimiterDialog::isSpeedLimitEnabled() const
{
    return ui->enableCheckBox->isChecked();
}

void SpeedLimiterDialog::setSpeedLimitEnabled(bool enabled)
{
    ui->enableCheckBox->setChecked(enabled);
}

void SpeedLimiterDialog::setSpeedLimitKBps(int kbPerSec)
{
    ui->speedLimitSpinBox->setValue(kbPerSec);
}
