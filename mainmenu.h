#ifndef MAINMENU_H
#define MAINMENU_H

#include <QObject>
#include <QMenuBar>
#include "configdockwidget.h"

class MainMenu : public QObject
{
    Q_OBJECT
public:
    explicit MainMenu(QMenuBar *menuBar, QObject *parent = nullptr);
    virtual ~MainMenu() = default;

    void updateMenuState(ConfigDockWidget *activeWidget);

signals:
    void openFileRequested();

private:
    void initFileMenu(QMenuBar *menuBar);
    void initEditMenu(QMenuBar *menuBar);

    QAction *saveAction = nullptr;
    QAction *saveAsAction = nullptr;

    QMenu *editMenu = nullptr;
    void onEditMenuAboutToShow();
    QList<QAction*> activeTextActions() const;

};

#endif // MAINMENU_H
