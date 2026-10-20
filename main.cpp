#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("LogicBox"));
    QCoreApplication::setApplicationName(QStringLiteral("lbcfg"));
    QCoreApplication::setApplicationVersion(MainWindow::getLibraryVersion());
    MainWindow::initLanguage(a);

    MainWindow w;
    w.show();
    return a.exec();
}
