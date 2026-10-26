#include "appsettings.h"
#include <QDir>
#include <QStandardPaths>

#ifdef QT_DEBUG
int AppSettings::defaultLogLevel = 4;
#else
int AppSettings::defaultLogLevel = 3;
#endif

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

// --- YAML Auto-indent ---
bool AppSettings::getAutoIndentYaml()
{
    return get<bool>(autoIndentYaml, true);
}

void AppSettings::setAutoIndentYaml(const bool v)
{
    set(autoIndentYaml, v);
}

void AppSettings::clearAutoIndentYaml()
{
    clear(autoIndentYaml);
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

// Валидатор для проверки корректности индекса Level (от Critical=0 до Debug=4)
bool isValidLogLevel(int val) {
    return val >= 0 && val <= 4;
}

// --- AppMinLogLevel ---
int AppSettings::getAppMinLogLevel() {
    return get<int>(appMinLogLevelKey, defaultLogLevel, isValidLogLevel);
}

void AppSettings::setAppMinLogLevel(const int level) {
    set<int>(appMinLogLevelKey, level, isValidLogLevel);
}

void AppSettings::clearAppMinLogLevel() {
    clear(appMinLogLevelKey);
}

// --- PlcMinLogLeve ---
int AppSettings::getPlcMinLogLevel() {
    return get<int>(plcMinLogLevelKey, defaultLogLevel, isValidLogLevel);
}

void AppSettings::setPlcMinLogLevel(const int level) {
    set<int>(plcMinLogLevelKey, level, isValidLogLevel);
}

void AppSettings::clearPlcMinLogLevel() {
    clear(plcMinLogLevelKey);
}

// --- LogBlockCount ---
bool isValidLogBlockCount(int val) {
    return val >= 100 && val <= 500000;
}

int AppSettings::getMaxLogBlockCount() {
    return get<int>(maxLogBlockCountKey, 10000, isValidLogBlockCount); // 10000 — дефолтное значение
}

void AppSettings::setMaxLogBlockCount(const int count) {
    set<int>(maxLogBlockCountKey, count, isValidLogBlockCount);
}

void AppSettings::clearMaxLogBlockCount() {
    clear(maxLogBlockCountKey);
}

// --- AppLanguage ---
QString AppSettings::getAppLanguage() {
    // По умолчанию возвращаем "ru" (или "system" для системного)
    return get<QString>(appLanguageKey, QStringLiteral("system"));
}

void AppSettings::setAppLanguage(const QString &lang) {
    set<QString>(appLanguageKey, lang);
}

void AppSettings::clearAppLanguage() {
    clear(appLanguageKey);
}

// Валидатор для количества повторов
bool isValidRetryCount(int val) {
    return val >= 0 && val <= 5;
}

// --- Max Firmware Retries ---
int AppSettings::getMaxFirmwareRetries() {
    return get<int>(maxFirmwareRetriesKey, 3, isValidRetryCount);
}

void AppSettings::setMaxFirmwareRetries(const int count) {
    set<int>(maxFirmwareRetriesKey, count, isValidRetryCount);
}

void AppSettings::clearMaxFirmwareRetries() {
    clear(maxFirmwareRetriesKey);
}

// --- Max Config Read Retries ---
int AppSettings::getMaxConfigReadRetries() {
    return get<int>(maxConfigReadRetriesKey, 3, isValidRetryCount);
}

void AppSettings::setMaxConfigReadRetries(const int count) {
    set<int>(maxConfigReadRetriesKey, count, isValidRetryCount);
}

void AppSettings::clearMaxConfigReadRetries() {
    clear(maxConfigReadRetriesKey);
}

