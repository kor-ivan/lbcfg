#include "commandmanager.h"
#include <QApplication>
#include "logmanager.h"

CommandManager::CommandManager()
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

QAction *CommandManager::getSaveAsAction() const
{
    return saveAsAction;
}

void CommandManager::setSaveAsAction(QAction *newSaveAsAction)
{
    saveAsAction = newSaveAsAction;
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

