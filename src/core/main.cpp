#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    QCoreApplication::setOrganizationName("Advanced");
    QCoreApplication::setApplicationName("IDMApp");

    return a.exec();
}
