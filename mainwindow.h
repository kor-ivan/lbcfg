#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QTextEdit>
#include <QProgressBar>
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

    QProgressBar *firmwareProgressBar = nullptr;
    void startFirmware(const QString &ipv6, const QString &filePath, const int &slot=-1);


protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;


private slots:
    void onDeviceSelected(const QString& ipv6, const QString& name);
    void getlbcfg (const QString &ipv6, const QString &name);
    void CreateConfig(const QString &ipv6, const QString &name, const QString &content = {});


};
#endif // MAINWINDOW_H
