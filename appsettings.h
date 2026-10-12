#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>

class FirmwareRepositoryDialog;
class SettingsDialog;

class AppSettings
{
    friend class FirmwareRepositoryDialog;
    friend class SettingsDialog;

public:
    static QString firmwareRepositoryRoot();
    static int getFirmwareStrtegy();
    static bool getFirmwareIgnoreCPU();
    static int getMaxFlowLevel();
    static int getStyleYamlArray();

private:
    static constexpr const char *FirmwareRepositoryKey = "Firmware/repositoryRoot";
    static void setFirmwareRepositoryRoot(const QString &path);
    static void clearFirmwareRepositoryRoot();

    static inline const QLatin1StringView firmwareStrtegy{"Firmware/strategy"};
    static void setFirmwareStrtegy(const int &str);
    static void clearFirmwareStrtegy();

    static inline const QLatin1StringView firmwareIgnoreCPU{"Firmware/ignoreCPU"};
    static void setFirmwareIgnoreCPU(const bool a);
    static void clearFirmwareIgnoreCPU();

    static inline const QLatin1StringView maxFlowLevel{"YAML/maxFlowLevel"};
    static void setMaxFlowLevel(const int level);
    static void clearMaxFlowLevel();

    static inline const QLatin1StringView styleYamlArray{"YAML/styleYamlArray"};
    static void setStyleYamlArray(const int style);
    static void clearStyleYamlArray();
};

#endif // APPSETTINGS_H
