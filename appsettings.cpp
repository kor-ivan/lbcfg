#include "appsettings.h"

#include <QDir>
#include <QStandardPaths>

QSettings AppSettings::getSettings(){
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDataPath);
    return QSettings(appDataPath + QStringLiteral("/settings.ini"), QSettings::IniFormat);
}

// --- Firmware Repository Root ---
QString AppSettings::firmwareRepositoryRoot()
{
    const QString value = getSettings().value(FirmwareRepositoryKey).toString().trimmed();
    if (value.isEmpty())
        return {};

    return QDir::cleanPath(QDir::fromNativeSeparators(value));
}

void AppSettings::setFirmwareRepositoryRoot(const QString &path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));

    QSettings settings = getSettings();
    if (normalized.isEmpty() || normalized == QStringLiteral("."))
        settings.remove(FirmwareRepositoryKey);
    else
        settings.setValue(FirmwareRepositoryKey, normalized);

    settings.sync();
}

void AppSettings::clearFirmwareRepositoryRoot()
{
    clear(FirmwareRepositoryKey);
}

// --- Firmware Strategy ---
bool isValidFirmwareStrtegy(int val)
{
    return val >= 0 && val <= 3;
}

int AppSettings::getFirmwareStrtegy()
{
    return get<int>(firmwareStrtegy, 0, isValidFirmwareStrtegy);
}

void AppSettings::setFirmwareStrtegy(const int &str)
{
    set(firmwareStrtegy, str, isValidFirmwareStrtegy);
}

void AppSettings::clearFirmwareStrtegy()
{
    clear(firmwareStrtegy);
}

// --- Firmware Ignore CPU ---
bool AppSettings::getFirmwareIgnoreCPU()
{
    return get<bool>(firmwareIgnoreCPU, false);
}

void AppSettings::setFirmwareIgnoreCPU(const bool a)
{
    set<bool>(firmwareIgnoreCPU, a);
}

void AppSettings::clearFirmwareIgnoreCPU()
{
    clear(firmwareIgnoreCPU);
}

// --- Max Flow Level ---
bool isValidFlowLevel(int val)
{
    return val >= 1 && val <= 5;
}

int AppSettings::getMaxFlowLevel()
{
    return get<int>(maxFlowLevel, 2, isValidFlowLevel);
}

void AppSettings::setMaxFlowLevel(const int level)
{
    set<int>(maxFlowLevel, level, isValidFlowLevel);
}

void AppSettings::clearMaxFlowLevel()
{
    clear(maxFlowLevel);
}

// --- Style Yaml Array ---
bool isValidStyleYaml(int val)
{
    return val >= 1 && val <= 2;
}

int AppSettings::getStyleYamlArray()
{
    return get<int>(styleYamlArray, 2, isValidStyleYaml);
}

void AppSettings::setStyleYamlArray(const int style)
{
    set<int>(styleYamlArray, style, isValidStyleYaml);
}

void AppSettings::clearStyleYamlArray()
{
    clear(styleYamlArray);
}
