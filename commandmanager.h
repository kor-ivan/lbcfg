#ifndef COMMANDMANAGER_H
#define COMMANDMANAGER_H

#include <QObject>
#include <QPointer>
#include "configdockwidget.h"

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
    void checkConfigDockWidget(QDockWidget *dock);

    QAction *getSaveAction() const;
    void setSaveAction(QAction *newSaveAction);

    QAction *getSaveAsAction() const;
    void setSaveAsAction(QAction *newSaveAsAction);

    void getLogMenu(const plcManager::CommandContext &ctx, QMenu *parentMenu);

    QAction *getConfAction() const;
    void setConfAction(QAction *newConfAction);

signals:
    void activeConfDockWidgetChanged(ConfigDockWidget *newWidget);

private:
    CommandManager();
    ~CommandManager() = default;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    plcManager *lbplc = nullptr;
    QPointer<ConfigDockWidget> activeConfDockWidget = nullptr;

    QAction *saveAction = nullptr;
    QAction *saveAsAction = nullptr;
    QPointer<QAction> confAction = nullptr;

};

#endif // COMMANDMANAGER_H
