#include "mainmenu.h"
#include "commandmanager.h"
#include <QApplication>

MainMenu::MainMenu(QMenuBar *menuBar, QObject *parent)
    : QObject{parent}
{
    initFileMenu(menuBar);
    initEditMenu(menuBar);

    connect(CommandManager::instance(), &CommandManager::activeConfDockWidgetChanged,
            this, &MainMenu::updateMenuState);
}

void MainMenu::updateMenuState(ConfigDockWidget *activeWidget)
{
    if (!activeWidget){
        saveAction->setEnabled(false);
        saveAsAction->setEnabled(false);
        saveAction->setText(tr("&Сохранить"));
        saveAsAction->setText(tr("Сохранить &как..."));
        return;
    }
    QString plcName = activeWidget->getPlcName();
    saveAction->setEnabled(true);
    saveAction->setText(QString("&Сохранить %1").arg(plcName));
    saveAsAction->setEnabled(true);
    saveAsAction->setText(QString("Сохранить %1 &как...").arg(plcName));
}

void MainMenu::initFileMenu(QMenuBar *menuBar)
{
    QMenu *fileMenu = menuBar->addMenu("&Файл");
    QAction *openAction = fileMenu->addAction("&Открыть...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainMenu::openFileRequested);

    saveAction = fileMenu->addAction("&Сохранить");
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setEnabled(false);
    connect(saveAction, &QAction::triggered, [](){
        if (!CommandManager::instance()->isNullActiveConfDockWidget())
            CommandManager::instance()->
                getActiveConfDockWidget()->saveFile();
    });
    CommandManager::instance()->setSaveAction(saveAction);

    saveAsAction = fileMenu->addAction(tr("Сохранить &как..."));
    saveAsAction->setEnabled(false);
    connect(saveAsAction, &QAction::triggered, [](){
        if (!CommandManager::instance()->isNullActiveConfDockWidget())
            CommandManager::instance()->
                getActiveConfDockWidget()->saveFileAs();
    });
    CommandManager::instance()->setSaveAsAction(saveAsAction);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(tr("&Выход"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void MainMenu::initEditMenu(QMenuBar *menuBar)
{
    editMenu = menuBar->addMenu("&Правка");
    connect(editMenu, &QMenu::aboutToShow, this, &MainMenu::onEditMenuAboutToShow);
}

void MainMenu::onEditMenuAboutToShow()
{
    editMenu->clear();
    QList<QAction*> actions = activeTextActions();

    if (actions.isEmpty()) {
        QAction *emptyAct = editMenu->addAction("Нет активного редактора");
        emptyAct->setEnabled(false);
        return;
    }

    for (QAction *act : actions) {
        editMenu->addAction(act);
    }
}

QList<QAction *> MainMenu::activeTextActions() const
{
    // Запрашиваем у Медиатора просто активный виджет
    ConfigDockWidget *active = CommandManager::instance()->getActiveConfDockWidget();
    if (!active) return QList<QAction*>();

    // Ищем в нем текстовый редактор
    QTextEdit *editor = active->getEditor();
    if (!editor) return QList<QAction*>();

    // Генерируем стандартное контекстное меню редактора, чтобы «украсть» из него настроенные экшены
    QMenu *tempMenu = editor->createStandardContextMenu();
    QList<QAction*> actions = tempMenu->actions();

    // Привязываем экшены к editor, чтобы они не удалились вместе с tempMenu
    for (QAction *act : actions) {
        if (act) act->setParent(editor);
    }

    tempMenu->deleteLater(); // Удаляем временную оболочку меню
    return actions;
}
