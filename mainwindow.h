#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include "lbcfg_global.h"

class DeviceTreeDockWidget;
class DiscoverDockWidget;
class LogDockWidget;
class WatchDockWidget;
class ConfigDockWidget;
class QLabel;

namespace plc {
struct CommandContext; // Поддержка вложенной структуры контекста
}

class MainWindowPrivate;

class LBCFG_CORE_EXPORT MainWindow : public QMainWindow
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
    WatchDockWidget* createWatchDockWidget(const plc::CommandContext &ctx);
    QList<ConfigDockWidget*> getConfigDocks() const;
    QList<WatchDockWidget*> getWatchDocks() const;

    void editFirmwareRepositorySettings();

    static QString getLibraryVersion();

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    MainWindowPrivate *p;
};
#endif // MAINWINDOW_H
