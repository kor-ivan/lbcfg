#include "mainwindow.h"
#include <QTranslator>
#include <QLibraryInfo>
#include <QApplication>
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("LogicBox"));
    QCoreApplication::setApplicationName(QStringLiteral("lbcfg"));
    QCoreApplication::setApplicationVersion(QString(APP_VERSION_STRING));
    // QSettings::setDefaultFormat(QSettings::NativeFormat);

    QTranslator qtTranslator;
    if (qtTranslator.load("qtbase_ru", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        a.installTranslator(&qtTranslator);
    MainWindow w;
    w.show();
    return a.exec();
}
