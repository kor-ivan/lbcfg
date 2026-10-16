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

// Валидатор для проверки, что число соответствует диапазону перечисления Qt::GlobalColor
bool isValidQtColor(int val)
{
    // От Qt::color0 (0) до Qt::transparent (18)
    return val >= 0 && val <= 18;
}

// --- YAML Color Root Keys ---
int AppSettings::getColorRootKeys()
{
    // По умолчанию: Qt::darkGreen (14)
    return get<int>(colorRootKeys, static_cast<int>(Qt::darkGreen), isValidQtColor);
}

void AppSettings::setColorRootKeys(const int color)
{
    set(colorRootKeys, color, isValidQtColor);
}

void AppSettings::clearColorRootKeys()
{
    clear(colorRootKeys);
}

// --- YAML Color Inner Keys ---
int AppSettings::getColorInnerKeys()
{
    // По умолчанию: Qt::blue (9)
    return get<int>(colorInnerKeys, static_cast<int>(Qt::blue), isValidQtColor);
}

void AppSettings::setColorInnerKeys(const int color)
{
    set(colorInnerKeys, color, isValidQtColor);
}

void AppSettings::clearColorInnerKeys()
{
    clear(colorInnerKeys);
}

// --- YAML Color Punctuation ---
int AppSettings::getColorPunctuation()
{
    // По умолчанию: Qt::black (2)
    return get<int>(colorPunctuation, static_cast<int>(Qt::black), isValidQtColor);
}

void AppSettings::setColorPunctuation(const int color)
{
    set(colorPunctuation, color, isValidQtColor);
}

void AppSettings::clearColorPunctuation()
{
    clear(colorPunctuation);
}

// --- YAML Color Values ---
int AppSettings::getColorValues()
{
    // По умолчанию: Qt::darkRed (13)
    return get<int>(colorValues, static_cast<int>(Qt::darkRed), isValidQtColor);
}

void AppSettings::setColorValues(const int color)
{
    set(colorValues, color, isValidQtColor);
}

void AppSettings::clearColorValues()
{
    clear(colorValues);
}

// --- YAML Color Lists ---
int AppSettings::getColorLists()
{
    // По умолчанию: Qt::magenta (11)
    return get<int>(colorLists, static_cast<int>(Qt::magenta), isValidQtColor);
}

void AppSettings::setColorLists(const int color)
{
    set(colorLists, color, isValidQtColor);
}

void AppSettings::clearColorLists()
{
    clear(colorLists);
}

// --- YAML Color Strings ---
int AppSettings::getColorStrings()
{
    // По умолчанию: Qt::darkCyan (15)
    return get<int>(colorStrings, static_cast<int>(Qt::darkCyan), isValidQtColor);
}

void AppSettings::setColorStrings(const int color)
{
    set(colorStrings, color, isValidQtColor);
}

void AppSettings::clearColorStrings()
{
    clear(colorStrings);
}

// --- YAML Color Comments ---
int AppSettings::getColorComments()
{
    // По умолчанию: Qt::gray (5)
    return get<int>(colorComments, static_cast<int>(Qt::gray), isValidQtColor);
}

void AppSettings::setColorComments(const int color)
{
    set(colorComments, color, isValidQtColor);
}

void AppSettings::clearColorComments()
{
    clear(colorComments);
}
