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
            checkDockWidget(foundDock);
        }
    });
}

QString CommandManager::toBold(const QString &text)
{
    return QString("<b>%1</b>").arg(text);
}

WatchDockWidget *CommandManager::getActiveWatchDockWidget() const
{
    return activeWatchDockWidget.get();
}

void CommandManager::getUptimeAction(const plcManager::CommandContext &ctx, QMenu *menu)
{
    qDebug() << "into getUptimeAction";
    QAction *getUptime = menu->addAction("Время работы");
    connect(getUptime, &QAction::triggered, this, [this, ctx]() {
        lbplc->lbc_executeCommand(ctx, {"get", "sys.uptime"}, "Время работы", [ctx, this](const QStringList& res) {
            QString uptime = res.isEmpty() ? toBold("none") : toBold(res.at(0));
            return QString("Время работы %1 %2 сек").arg(ctx.displayName(), uptime);
        });
    });
}

void CommandManager::getRestartAction(const plcManager::CommandContext &ctx, QMenu *menu)
{
    QAction *restart = menu->addAction("Перезагрузить");
    connect(restart, &QAction::triggered, this, [this, ctx]() {
        lbplc->lbc_executeCommand(ctx, {"set", "sys.restart=1"}, "Перезагрузка", [ctx](const QStringList&) {
            return QString("Команда на перезагрузку %1 отправлена").arg(ctx.displayName());
        });
    });
}

void CommandManager::getFlashAction(const plcManager::CommandContext &ctx, QMenu *menu)
{
    QAction *flash = menu->addAction("Загрузить прошивку ...");
    connect(flash, &QAction::triggered, this, [this, ctx](){
        emit requestFlash(ctx);
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
    connect(logAll, &QAction::triggered, logAll, [this, ctx](){
        lbplc->startLog(ctx, "a");
    });

    QAction *logLast100 = logMenu->addAction("Запросить 100 сообщений");
    connect(logLast100, &QAction::triggered, logLast100, [this, ctx](){
        lbplc->startLog(ctx, "a100");
    });

    QAction *logLast100f = logMenu->addAction("Запросить 100 и следовать");
    connect(logLast100f, &QAction::triggered, logLast100f, [this, ctx](){
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

void CommandManager::checkDockWidget(QDockWidget *dock)
{
    ConfigDockWidget *configDock = qobject_cast<ConfigDockWidget*>(dock);
    if (configDock) {
        debugApp() << "Выбран ConfigDockWidget: " << configDock;
        activeConfDockWidget = configDock;
        confAction = configDock->getConfigureAction();
        emit activeConfDockWidgetChanged(configDock);
    }
    WatchDockWidget *watchDock = qobject_cast<WatchDockWidget*>(dock);
    if (watchDock){
        debugApp() << "Выбран WatchDockWidget: " << watchDock;
        activeWatchDockWidget = watchDock;
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

