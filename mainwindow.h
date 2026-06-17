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
    ConfigDockWidget* findActiveConfigDockWidget();

protected:
    void showEvent(QShowEvent *event) override;


private slots:
    void onDeviceSelected(const QString& ipv6, const QString& name);
    void getlbcfg (const QString &ipv6, const QString &name);
    void CreateConfig(const QString &ipv6, const QString &name, const QString &content = {});
    void onSaveConfigAsTriggered();

};
#endif // MAINWINDOW_H
