#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QObject>
#include <QLabel>
#include <QComboBox>

class QListWidget;
class QStackedWidget;
class MainWindow;
class QCheckBox;

class SettingsDialog  : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    virtual ~SettingsDialog() = default;

private slots:
    void onCategoryChanged(int index);
    void saveSettings();

private:
    MainWindow* p_mainWindow = nullptr;
    void setupUi();

    QListWidget *sidebarMenu = nullptr;
    QStackedWidget *pagesContainer = nullptr;
    QComboBox *strategyCombo = nullptr;
    QCheckBox *ignoreCpuModuleCheck = nullptr;
    QComboBox *flowLevelCombo = nullptr;
    QComboBox *styleYamlCombo = nullptr;

    QWidget* createFirmwareSettings();
    QWidget* createYAMLSettings();
    QWidget* createLogSettings();
    QWidget* createLanguageSettings();

    void updateRepoStatusLabel(QLabel *label);

    QWidget* createSettingsLine(QWidget *mainWidget,
                                QPushButton **outResetButton = nullptr,
                                QLabel **outStatusLabel = nullptr);

    template <typename T, typename AppSetter>
    void updateSettings(QWidget* widget, T newValue, T oldValue, AppSetter setter){
        if (!widget)
            return;
        if (newValue != oldValue)
            setter(newValue);
    }

};

#endif // SETTINGSDIALOG_H
