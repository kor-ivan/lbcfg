#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include "firmwarewidget.h"
#include "devicetreedockwidget.h"
#include "discoverdockwidget.h"
#include "configdockwidget.h"
#include "plcmanager.h"
#include "mainmenu.h"
#include "logdockwidget.h"
#include "watchdockwidget.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    QPointer<DeviceTreeDockWidget> getTreeDock() const;
    QPointer<DiscoverDockWidget> getDiscoverDock() const;
    QPointer<LogDockWidget> getLogDock() const;

    DeviceTreeDockWidget *createTreeDockWidget();
    DiscoverDockWidget* createDiscoverDockWidget();
    LogDockWidget* createLogDockWidget();
    WatchDockWidget* createWatchDockWidget(const QString &name, const QString &ipv6 = {});
    QList<ConfigDockWidget*> getConfigDocks() const;
    QList<WatchDockWidget*> getWatchDocks() const;

private:
    plcManager *lbplc = nullptr;
    QPointer<DeviceTreeDockWidget> treeDock = nullptr;
    QPointer<DiscoverDockWidget> discoverDock = nullptr;
    QPointer<LogDockWidget> logDock = nullptr;
    QMap<QString, ConfigDockWidget*> configDocks;
    QMap<QString, WatchDockWidget*> watchDocks;

    ConfigDockWidget* CreateConfDockWidget(const QString &key, const QString &name);

    MainMenu *menu = nullptr;

    FirmwareWidget *fwWidget = nullptr;
    void CreateConfig(const QString &ipv6, const QString &name, const QString &content = {});

    QList<QDockWidget*> getDocksInArea(Qt::DockWidgetArea area) const;
    void tabifyDockWidgetTo(QDockWidget *dock, Qt::DockWidgetArea area);

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

};
#endif // MAINWINDOW_H
