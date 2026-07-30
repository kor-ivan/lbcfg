#include "commandmanager.h"
#include <QApplication>
#include "logmanager.h"
#include "qmenu.h"

CommandManager::CommandManager() :
    lbplc(plcManager::instanse())
{
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *oldFocus, QWidget *newFocus){
        Q_UNUSED(oldFocus);
        if (!newFocus) return;
        QWidget *parentCheck = newFocus;
        QDockWidget *foundDock = nullptr;
        while (parentCheck) {
            foundDock = qobject_cast<QDockWidget*>(parentCheck);
            if (foundDock)
                break;
            parentCheck = parentCheck->parentWidget();
        }
        if (foundDock){
            checkConfigDockWidget(foundDock);
        }
    });
}

QAction *CommandManager::getConfAction() const
{
    return confAction.get();
}

void CommandManager::setConfAction(QAction *newConfAction)
{
    confAction = newConfAction;
}

QAction *CommandManager::getSaveAsAction() const
{
    return saveAsAction;
}

void CommandManager::setSaveAsAction(QAction *newSaveAsAction)
{
    saveAsAction = newSaveAsAction;
}

void CommandManager::getLogMenu(const plcManager::CommandContext &ctx, QMenu *parentMenu, const QString &text)
{
    QMenu *logMenu = parentMenu->addMenu(text);
    QAction *logAll = logMenu->addAction("Запросить весь лог");
    connect(logAll, &QAction::triggered, this, [this, ctx](){
        lbplc->startLog(ctx, "a");
    });

    QAction *logLast100 = logMenu->addAction("Запросить 100 сообщений");
    connect(logLast100, &QAction::triggered, this, [this, ctx](){
        lbplc->startLog(ctx, "a100");
    });

    QAction *logLast100f = logMenu->addAction("Запросить 100 и следовать");
    connect(logLast100f, &QAction::triggered, this, [this, ctx](){
        lbplc->startLog(ctx, "a100f");
    });
}

QAction *CommandManager::getSaveAction() const
{
    return saveAction;
}

void CommandManager::setSaveAction(QAction *newSaveAction)
{
    saveAction = newSaveAction;
}

void CommandManager::checkConfigDockWidget(QDockWidget *dock)
{
    ConfigDockWidget *configDock = qobject_cast<ConfigDockWidget*>(dock);
    if (configDock) {
        debugApp() << "Выбран ConfigDockWidget: " << configDock;
        activeConfDockWidget = configDock;
        emit activeConfDockWidgetChanged(configDock);
    }
}


ConfigDockWidget *CommandManager::getActiveConfDockWidget() const
{
    return activeConfDockWidget.get();
}

bool CommandManager::isNullActiveConfDockWidget() const
{
    if (!activeConfDockWidget)
        return true;
    return false;
}

void CommandManager::resetActiveConfDockWidget()
{
    activeConfDockWidget.clear();
    emit activeConfDockWidgetChanged(nullptr);
}

