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


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    QPointer<DeviceTreeDockWidget> getTreeDock() const;

    QPointer<DiscoverDockWidget> getDiscoverDock() const;
    DeviceTreeDockWidget *createTreeDockWidget();
    DiscoverDockWidget* createDiscoverDockWidget();

    QList<ConfigDockWidget*> getConfigDocks() const;

private:
    plcManager *lbplc = nullptr;
    QPointer<DeviceTreeDockWidget> treeDock = nullptr;
    QPointer<DiscoverDockWidget> discoverDock = nullptr;
    QMap<QString, ConfigDockWidget*> configDocks;

    ConfigDockWidget* CreateConfDockWidget(const QString &key, const QString &name);

    MainMenu *menu = nullptr;

    FirmwareWidget *fwWidget = nullptr;
    void CreateConfig(const QString &ipv6, const QString &name, const QString &content = {});

    QList<QDockWidget*> getDocksInArea(Qt::DockWidgetArea area) const;

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

};
#endif // MAINWINDOW_H
