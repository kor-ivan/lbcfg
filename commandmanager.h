#ifndef COMMANDMANAGER_H
#define COMMANDMANAGER_H

#include <QObject>
#include <QPointer>
#include "configdockwidget.h"
#include "watchdockwidget.h"
#include "firmwareanalyzer.h"

class CommandManager : public QObject
{
    Q_OBJECT
public:
    // explicit CommandManager(QObject *parent = nullptr);
    // Синглтон Майерса
    static CommandManager* instance() {
        static CommandManager inst;
        return &inst;
    }

    ConfigDockWidget* getActiveConfDockWidget() const;
    bool isNullActiveConfDockWidget() const;
    void resetActiveConfDockWidget();
    void checkDockWidget(QDockWidget *dock);

    QAction *getSaveAction() const;
    void setSaveAction(QAction *newSaveAction);

    QAction *getSaveAsAction() const;
    void setSaveAsAction(QAction *newSaveAsAction);

    void getLogMenu(const plcManager::CommandContext &ctx, QMenu *parentMenu,
                    const QString &text = "Запросить лог");

    QAction *getConfAction() const;
    void setConfAction(QAction *newConfAction);

    WatchDockWidget* getActiveWatchDockWidget() const;

    void getUptimeAction (const plcManager::CommandContext &ctx, QMenu *menu);
    void getRestartAction (const plcManager::CommandContext &ctx, QMenu *menu);
    void getFlashAction (const plcManager::CommandContext &ctx, QMenu *menu);

    firmwareAnalyzer *getFirmwareAnalyzer() const;
    void setFirmwareAnalyzer(firmwareAnalyzer *newFirmwareAnalyzer);

signals:
    void activeConfDockWidgetChanged(ConfigDockWidget *newWidget);
    void requestFlash(const plcManager::CommandContext &ctx);

private:
    CommandManager();
    ~CommandManager() = default;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    plcManager *lbplc = nullptr;
    QPointer<ConfigDockWidget> activeConfDockWidget = nullptr;
    QPointer<WatchDockWidget> activeWatchDockWidget = nullptr;

    QAction *saveAction = nullptr;
    QAction *saveAsAction = nullptr;
    QPointer<QAction> confAction = nullptr;

    inline QString toBold(const QString &text);
    firmwareAnalyzer *m_firmwareAnalyzer = nullptr;
};

#endif // COMMANDMANAGER_H
