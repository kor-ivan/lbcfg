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

private:
    static constexpr const char *FirmwareRepositoryKey = "Firmware/repositoryRoot";
    static void setFirmwareRepositoryRoot(const QString &path);
    static void clearFirmwareRepositoryRoot();

    static inline const QLatin1StringView firmwareStrtegy{"Firmware/strategy"};
    static void setFirmwareStrtegy(const int &str);
    static void clearFirmwareStrtegy();
};

#endif // APPSETTINGS_H
