#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QTextEdit>
#include "devicetreedockwidget.h"
#include "discoverdockwidget.h"
#include "configdockwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private:
    DeviceTreeDockWidget *treeDock = nullptr;
    DiscoverDockWidget *discoverDock = nullptr;
    QMap<QString, ConfigDockWidget*> configDocks;

    ConfigDockWidget* activeConfDockWidget = nullptr;
    // bool isConfigDockWidget();
    void checkConfigDockWidget(QDockWidget *dock);
    // void SaveConfigAs(ConfigDockWidget* activeDock);

    ConfigDockWidget* CreateConfDockWidget(const QString &key, const QString &name);
    QAction *saveFileAs = nullptr;
    QAction *saveFile = nullptr;


protected:
    void showEvent(QShowEvent *event) override;


private slots:
    void onDeviceSelected(const QString& ipv6, const QString& name);
    void getlbcfg (const QString &ipv6, const QString &name);
    void CreateConfig(const QString &ipv6, const QString &name, const QString &content = {});
    // void onSaveFileTriggered();

};
#endif // MAINWINDOW_H
