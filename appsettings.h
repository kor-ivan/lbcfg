#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>

class FirmwareRepositoryDialog;

class AppSettings
{
    friend class FirmwareRepositoryDialog;
public:
    static QString firmwareRepositoryRoot();

private:
    static constexpr const char *FirmwareRepositoryKey = "Firmware/repositoryRoot";
    static void setFirmwareRepositoryRoot(const QString &path);
    static void clearFirmwareRepositoryRoot();
};

#endif // APPSETTINGS_H
