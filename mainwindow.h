#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QTextEdit>
#include "devicetreewidget.h"
#include "discoverwidget.h"
#include "configdockwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    DeviceTreeWidget *treeDock = nullptr;
    DiscoverWidget *discoverDock = nullptr;
    QMap<QString, ConfigDockWidget*> configDocks;

protected:
    void showEvent(QShowEvent *event) override;


private slots:
    void onDeviceSelected(const QString& ipv6, const QString& name);
    void getlbcfg (const QString &ipv6, const QString &name);

};
#endif // MAINWINDOW_H
