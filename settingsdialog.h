#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QObject>
#include <QLabel>
#include <QComboBox>

class QListWidget;
class QStackedWidget;
class MainWindow;

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

    QWidget* createFirmwareSettings();
    QWidget* createYAMLSettings();
    QWidget* createLogSettings();
    QWidget* createLanguageSettings();

    void updateRepoStatusLabel(QLabel *label);

};

#endif // SETTINGSDIALOG_H
