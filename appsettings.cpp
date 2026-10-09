#include "appsettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

static QSettings getSettings(){
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDataPath);
    return QSettings(appDataPath + QStringLiteral("/settings.ini"), QSettings::IniFormat);
}

QString AppSettings::firmwareRepositoryRoot()
{
    const QString value = getSettings().value(QString::fromLatin1(FirmwareRepositoryKey)).toString().trimmed();
    if (value.isEmpty())
        return {};

    return QDir::cleanPath(QDir::fromNativeSeparators(value));
}

int AppSettings::getFirmwareStrtegy()
{
    const auto value = getSettings().value(firmwareStrtegy);
    if (value.isNull())
        return 0;
    bool ok = false;
    int strategy = value.toInt(&ok);
    if (ok)
        return strategy;
    return 0;
}

void AppSettings::setFirmwareRepositoryRoot(const QString &path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));

    QSettings settings = getSettings();
    if (normalized.isEmpty() || normalized == QStringLiteral("."))
        settings.remove(QString::fromLatin1(FirmwareRepositoryKey));
    else
        settings.setValue(QString::fromLatin1(FirmwareRepositoryKey), normalized);

    settings.sync();
}

void AppSettings::clearFirmwareRepositoryRoot()
{
    QSettings settings = getSettings();
    settings.remove(QString::fromLatin1(FirmwareRepositoryKey));
    settings.sync();
}

void AppSettings::setFirmwareStrtegy(const int &str)
{
    QSettings settings = getSettings();
    settings.setValue(firmwareStrtegy, str);
    settings.sync();
}

void AppSettings::clearFirmwareStrtegy()
{
    QSettings settings = getSettings();
    settings.remove(firmwareStrtegy);
    settings.sync();
}
