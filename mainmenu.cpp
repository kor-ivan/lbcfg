#include "mainmenu.h"
#include "commandmanager.h"
#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

MainMenu::MainMenu(MainWindow *mainWindow)
    : QObject{mainWindow}, p_mainWindow(mainWindow), p_menuBar(mainWindow->menuBar())
{

    initFileMenu(p_menuBar);
    initEditMenu(p_menuBar);
    initViewMenu(p_menuBar);
    initPlcMenu(p_menuBar);
    initHelpMenu(p_menuBar);

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

    QAction *newAction = fileMenu->addAction("&Новая конфигурация");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainMenu::newConfigurationRequested);

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

void MainMenu::initViewMenu(QMenuBar *menuBar)
{
    viewMenu = menuBar->addMenu("&Вид");
    connect(viewMenu, &QMenu::aboutToShow, this, &MainMenu::onViewMenuAboutToShow);
}

void MainMenu::initPlcMenu(QMenuBar *menuBar)
{
    plcMenu = menuBar->addMenu("&ПЛК");
    connect(plcMenu, &QMenu::aboutToShow, this, &MainMenu::onPlcMenuAboutToShow);
}

void MainMenu::initHelpMenu(QMenuBar *menuBar)
{
QMenu *helpMenu = menuBar->addMenu("&Справка");

    QAction *aboutAct = helpMenu->addAction(tr("&О программе..."));
    aboutAct->setStatusTip(tr("Показать информацию о приложении"));

    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(p_mainWindow,
                           tr("О программе lbcfg"),
                           tr("<h3>Конфигуратор ПЛК Logic Box</h3>"
                              "<p>Версия 1.0.0</p>"
                              "<p>Программа предназначена для сканирования устройств, "
                              "редактирования файлов конфигурации YAML/YML и безопасной "
                              "загрузки прошивок в ПЛК.</p>"
                              "<p>Данное программное обеспечение использует библиотеку Qt, "
                              "распространяемую на условиях лицензии GNU Lesser General Public License (LGPL) версии 3. "
                              "Вы имеете право пересобирать приложение с измененной версией библиотеки Qt в соответствии с условиями LGPLv3.</p>"
                              "<p>Подробную информацию о лицензии Qt можно найти в меню 'О библиотеке Qt'.</p>")
                           );
    });

    QAction *aboutQtAct = helpMenu->addAction("О библиотеке &Qt...");

    connect(aboutQtAct, &QAction::triggered, this, [this]() {
        QMessageBox::aboutQt(p_mainWindow, "О библиотеке Qt");
    });
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

void MainMenu::onViewMenuAboutToShow()
{
    viewMenu->clear();
    QAction *treeAct = viewMenu->addAction(tr("Дерево устройств"));
    treeAct->setCheckable(true);
    // Проверяем через геттер: если док создан и виден — ставим галочку
    bool treeExistsAndVisible = (p_mainWindow->getTreeDock() && p_mainWindow->getTreeDock()->isVisible());
    treeAct->setChecked(treeExistsAndVisible);

    connect(treeAct, &QAction::triggered, this, [this, treeExistsAndVisible]() {
        if (treeExistsAndVisible) {
            p_mainWindow->getTreeDock()->close(); // Закрываем (и уничтожаем благодаря WA_DeleteOnClose)
        } else {
            auto* dock = p_mainWindow->createTreeDockWidget(); // Создаем заново или открываем
            dock->show();
            dock->raise();
        }
    });

    QAction *discAct = viewMenu->addAction(tr("Поиск устройств"));
    discAct->setCheckable(true);
    bool discExistsAndVisible = (p_mainWindow->getDiscoverDock() && p_mainWindow->getDiscoverDock()->isVisible());
    discAct->setChecked(discExistsAndVisible);

    connect(discAct, &QAction::triggered, this, [this, discExistsAndVisible]() {
        if (discExistsAndVisible) {
            p_mainWindow->getDiscoverDock()->close();
        } else {
            auto* dock = p_mainWindow->createDiscoverDockWidget();
            dock->show();
            dock->raise();
        }
    });

    QAction *logAct = viewMenu->addAction(tr("Логи"));
    logAct->setCheckable(true);
    bool logExistsAndVisible = (p_mainWindow->getLogDock() &&
                                p_mainWindow->isVisible());
    logAct->setChecked(logExistsAndVisible);
    connect(logAct, &QAction::triggered, this, [this, logExistsAndVisible](){
        if (logExistsAndVisible)
            p_mainWindow->getLogDock()->close();
        else{
            auto *dock = p_mainWindow->createLogDockWidget();
            dock->show();
            dock->raise();
        }
    });

    QList<ConfigDockWidget*> openConf = p_mainWindow->getConfigDocks();

    if (!openConf.isEmpty()) {
        viewMenu->addSeparator();
        QMenu *confMenu = viewMenu->addMenu("Открытые конфигурации");

        for (ConfigDockWidget *dock : openConf) {
            QAction *docAct = confMenu->addAction(dock->getPlcName());

            // Если этот файл сейчас редактируется (активен) — ставим галочку
            if (CommandManager::instance()->getActiveConfDockWidget() == dock) {
                docAct->setCheckable(true);
                docAct->setChecked(true);
            }

            // По клику переключаемся на этот файл
            connect(docAct, &QAction::triggered, this, [dock]() {
                dock->show();
                dock->raise();
                dock->setFocus();
            });
        }
    }

    auto openWatch = p_mainWindow->getWatchDocks();
    QMenu *watchMenu = viewMenu->addMenu("Watch");

    QAction *createWatch = watchMenu->addAction("Создать новый");
    createWatch->setShortcut(QKeySequence("Ctrl+W"));
    connect(createWatch, &QAction::triggered, this, [this](){
        static QAtomicInt counter(0);
        QString str = QString("new %1").arg(counter.fetchAndAddRelaxed(1) + 1);
        WatchDockWidget* watch = p_mainWindow->createWatchDockWidget(str);
        watch->show();
        watch->raise();
        watch->setFocus();
    });

    if (!openWatch.isEmpty()){

        for (auto *watch : openWatch){
            QAction *watchAct = watchMenu->addAction(watch->getPlcName());

            if (CommandManager::instance()->getActiveWatchDockWidget() == watch){
                watchAct->setCheckable(true);
                watchAct->setChecked(true);
            }

            connect(watchAct, &QAction::triggered, this, [watch](){
                watch->show();
                watch->raise();
                watch->setFocus();
            });
        }
    }
}

void MainMenu::onPlcMenuAboutToShow()
{
    plcMenu->clear();
    QAction *discoverAction = plcMenu->addAction("Сканироавть");
    connect(discoverAction, &QAction::triggered, [this](){
        if (!p_mainWindow->getDiscoverDock().get()){
            auto dock = p_mainWindow->createDiscoverDockWidget();
            dock->show();
            dock->raise();
            dock->setFocus();
        }
        plcManager::instanse()->startDiscover();
    });

    QAction *confAction = CommandManager::instance()->getConfAction();
    if (confAction){

        confAction->setText(QString("Сконфигурировать %1")
                                .arg(CommandManager::instance()->getActiveConfDockWidget()->getPlcName()));
        plcMenu->addAction(confAction);
    }
    else{
        confAction = plcMenu->addAction("Сконфигурировать");
        confAction->setEnabled(false);
    }

    QMenu *logMenu = plcMenu->addMenu("Запросить лог у...");
    if (p_mainWindow->getDiscoverDock().get())
    {
        logMenu->setEnabled(true);
        QMap<QString, discover::lbinfo> ldmap = p_mainWindow->getDiscoverDock()->getLdmap();
        plcManager::CommandContext ctx;
        for (auto it = ldmap.begin(); it != ldmap.end(); ++it){
            ctx.ipv6 = it.key();
            CommandManager::instance()->getLogMenu(ctx, logMenu, it.value().name);
        }
    }else{
        logMenu->setEnabled(false);
    }
}

QList<QAction *> MainMenu::activeTextActions() const
{
    // Запрашиваем у Медиатора просто активный виджет
    ConfigDockWidget *active = CommandManager::instance()->getActiveConfDockWidget();
    if (!active) return QList<QAction*>();

    return active->activeTextActions();
}
