#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <signal.h>
#include <QApplication>

void handleCrash(int sig) {
    qCritical() << "Application crashed with signal:" << sig;
    exit(sig);
}
int main(int argc, char *argv[])
{
    signal(SIGSEGV, handleCrash); // Handle segmentation fault
    signal(SIGABRT, handleCrash);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    QCoreApplication::setOrganizationName("Advanced");
    QCoreApplication::setApplicationName("IDMApp");

    return a.exec();
}
