#include "about.h"
#include "ui_about.h"
#include <qevent.h>

/**
 * \brief Constructor for the AboutDialog class
 *
 * This function creates the AboutDialog and sets up the UI
 *
 * \param parent The parent widget
 */
AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &AboutDialog::closeDialog);
}

/**
 * \brief Destructor for the AboutDialog class
 *
 * This function cleans up the UI, which must be manually deleted
 */
AboutDialog::~AboutDialog()
{
    delete ui;
}
/**
 * \brief Reimplementation of the closeEvent function
 *
 * This function is called when the user requests to close the dialog. It simply
 * accepts the close event to allow the dialog to close.
 *
 * \param event The close event
 */
void AboutDialog::closeEvent(QCloseEvent *event)
{
    // Accept the close event to allow the dialog to close
    event->accept();
}


void AboutDialog::closeDialog()
{
    close(); // Trigger the close action
}
