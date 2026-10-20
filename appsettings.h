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

    static int getColorRootKeys();
    static int getColorInnerKeys();
    static int getColorPunctuation();
    static int getColorValues();
    static int getColorLists();
    static int getColorStrings();
    static int getColorComments();
    static bool getAutoIndentYaml();
    static int getAppMinLogLevel();
    static int getPlcMinLogLevel();
    static int getMaxLogBlockCount();
    static QString getAppLanguage();

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

    static inline const QLatin1StringView autoIndentYaml{"YAML/autoIndent"};
    static void setAutoIndentYaml(const bool v);
    static void clearAutoIndentYaml();

    static inline const QLatin1StringView colorRootKeys{"YAML_Color/rootKeys"};
    static void setColorRootKeys(const int color);
    static void clearColorRootKeys();

    static inline const QLatin1StringView colorInnerKeys{"YAML_Color/innerKeys"};
    static void setColorInnerKeys(const int color);
    static void clearColorInnerKeys();

    static inline const QLatin1StringView colorPunctuation{"YAML_Color/punctuation"};
    static void setColorPunctuation(const int color);
    static void clearColorPunctuation();

    static inline const QLatin1StringView colorValues{"YAML_Color/values"};
    static void setColorValues(const int color);
    static void clearColorValues();

    static inline const QLatin1StringView colorLists{"YAML_Color/lists"};
    static void setColorLists(const int color);
    static void clearColorLists();

    static inline const QLatin1StringView colorStrings{"YAML_Color/strings"};
    static void setColorStrings(const int color);
    static void clearColorStrings();

    static inline const QLatin1StringView colorComments{"YAML_Color/comments"};
    static void setColorComments(const int color);
    static void clearColorComments();

    static inline const QLatin1StringView appMinLogLevelKey{"Logging/appMinLevel"};
    static void setAppMinLogLevel(const int level);
    static void clearAppMinLogLevel();

    static inline const QLatin1StringView plcMinLogLevelKey{"Logging/plcMinLevel"};
    static void setPlcMinLogLevel(const int level);
    static void clearPlcMinLogLevel();

    static inline const QLatin1StringView maxLogBlockCountKey{"Logging/maxBlockCount"};
    static void setMaxLogBlockCount(const int count);
    static void clearMaxLogBlockCount();

    static inline const QLatin1StringView appLanguageKey{"Application/Language"};
    static void setAppLanguage(const QString &lang);
    static void clearAppLanguage();

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
    static int defaultLogLevel;
};

#endif // APPSETTINGS_H
