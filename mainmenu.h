#ifndef MAINMENU_H
#define MAINMENU_H

#include <QObject>
#include <QMenuBar>
#include "configdockwidget.h"
// #include "mainwindow.h"

class MainWindow;
class MainMenu : public QObject
{
    Q_OBJECT
public:
    explicit MainMenu(MainWindow *mainWindow = nullptr);
    virtual ~MainMenu() = default;

    void updateMenuState(ConfigDockWidget *activeWidget);

signals:
    void openFileRequested();

private:
    MainWindow *p_mainWindow = nullptr;
    QMenuBar   *p_menuBar    = nullptr;

    void initFileMenu(QMenuBar *menuBar);
    void initEditMenu(QMenuBar *menuBar);
    void initViewMenu(QMenuBar *menuBar);
    void initHelpMenu(QMenuBar *menuBar);

    QAction *saveAction = nullptr;
    QAction *saveAsAction = nullptr;

    QMenu *editMenu = nullptr;
    QMenu *viewMenu = nullptr;
    void onEditMenuAboutToShow();
    void onViewMenuAboutToShow();
    QList<QAction*> activeTextActions() const;

};

#endif // MAINMENU_H
