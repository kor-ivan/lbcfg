#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QSettings>

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
    static inline const QLatin1StringView FirmwareRepositoryKey{"Firmware/repositoryRoot"};
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

    static QSettings getSettings();

    template <typename T, typename Predicate = std::nullptr_t>
    static T get(QLatin1StringView key, const T &defaultValue = T(), Predicate isValid = nullptr){
        auto variant = getSettings().value(key);
        if (variant.isNull())
            return defaultValue;
        if (!variant.convert(QMetaType::fromType<T>()))
            return defaultValue;
        T val = variant.value<T>();
        if constexpr (std::is_same_v<T, QString>) {
            if (val.isEmpty() && !defaultValue.isEmpty())
                return defaultValue;
        }
        if constexpr (!std::is_same_v<Predicate, std::nullptr_t>){
            if (!isValid(val))
                return defaultValue;
        }
        return val;
    }

    template <typename T, typename Predicate = std::nullptr_t>
    static bool set(QLatin1StringView key, const T &value, Predicate isValid = nullptr){
        if constexpr (!std::is_same_v<Predicate, std::nullptr_t>) {
            if (!isValid(value))
                return false;
        }
        QSettings settings = getSettings();
        settings.setValue(key, value);
        settings.sync();
        return true;
    }

    static void clear(QLatin1StringView key){
        QSettings settings = getSettings();
        settings.remove(key);
        settings.sync();
    }
};

#endif // APPSETTINGS_H
