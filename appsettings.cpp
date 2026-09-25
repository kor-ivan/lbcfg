#include "appsettings.h"

#include <QDir>
#include <QSettings>
#include <QCoreApplication>

static QString getIniPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini");
}

QString AppSettings::firmwareRepositoryRoot()
{
    QSettings settings(getIniPath(), QSettings::IniFormat);
    const QString value = settings.value(QString::fromLatin1(FirmwareRepositoryKey)).toString().trimmed();
    if (value.isEmpty())
        return {};

    return QDir::cleanPath(QDir::fromNativeSeparators(value));
}

void AppSettings::setFirmwareRepositoryRoot(const QString &path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));

    QSettings settings(getIniPath(), QSettings::IniFormat);
    if (normalized.isEmpty() || normalized == QStringLiteral("."))
        settings.remove(QString::fromLatin1(FirmwareRepositoryKey));
    else
        settings.setValue(QString::fromLatin1(FirmwareRepositoryKey), normalized);

    settings.sync();
}

void AppSettings::clearFirmwareRepositoryRoot()
{
    QSettings settings(getIniPath(), QSettings::IniFormat);
    settings.remove(QString::fromLatin1(FirmwareRepositoryKey));
    settings.sync();
}
