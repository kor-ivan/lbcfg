#include "mainwindow.h"
#include <QDockWidget>
#include <QStatusBar>
#include <QLabel>
#include <QFileDialog>
#include <QApplication>
#include <QHelpEvent>
#include <QToolTip>
#include <QHBoxLayout>
#include <QMessageBox>
#include "commandmanager.h"



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 720);

    QWidget* dummy = new QWidget(this);
    setCentralWidget(dummy);
    dummy->hide(); // Скрываем, чтобы доки сомкнулись в центре

    lbplc = new plcManager(this);
    treeDock = new DeviceTreeDockWidget(this, lbplc);
    discoverDock = new DiscoverDockWidget(this, lbplc);
    // Разрешаем прикрепление ко всем сторонам: Left, Right, Top, Bottom
    treeDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    discoverDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Или QWidget, или QMdiArea
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    addDockWidget(Qt::RightDockWidgetArea, discoverDock);
    setDockNestingEnabled(true);

    connect(treeDock, &DeviceTreeDockWidget::requestFlash, this, [this]
            (const plcManager::CommandContext &ctx){
        QString filePath = QFileDialog::getOpenFileName(this, "Загрузить прошивку ...", "", "BIN Files (*.bin);;All Files (*)");
        if (!filePath.isEmpty()) {
            lbplc->startFirmware(ctx, filePath,
                                 "Загрузка уже выполняется, дождитесь окончания",
                                 QString("Загрузка прошивки в %1 ...").arg(ctx.displayName()));
        }
    });

    connect(treeDock, &DeviceTreeDockWidget::requestFlashAll, this, [this]
            (const plcManager::CommandContext &ctx){
        QString filePath = QFileDialog::getExistingDirectory(this, "Выберите директорию для прошивки ...", "", QFileDialog::DontResolveSymlinks);
        if (!filePath.isEmpty()) {
            lbplc->startFirmwareAll(ctx, filePath,
                                 "Загрузка уже выполняется, дождитесь окончания",
                                 QString("Загрузка прошивки в %1 ...").arg(ctx.displayName()));
        }
    });

    connect(treeDock, &DeviceTreeDockWidget::requestFboot, this, [this]
            (const plcManager::CommandContext &ctx){
                QString filePath = QFileDialog::getOpenFileName(this, "Загрузить fboot ...", "", "Fboot Files (*.fboot);;All Files (*)");                if (!filePath.isEmpty()) {
                    lbplc->startFirmware(ctx, filePath,
                                         "Загрузка уже выполняется, дождитесь окончания",
                                         QString("Загрузка fboot в %1 ...").arg(ctx.displayName()),
                                         "fboot");
                }
            });

    connect(discoverDock, &DiscoverDockWidget::newConfig,
            this, [this] (const QString &ipv6, const QString &name){
                CreateConfig(ipv6, name);
            }
            );

    connect(this, &QMainWindow::tabifiedDockWidgetActivated,
            CommandManager::instance(), &CommandManager::checkConfigDockWidget);

    // 1. Создаем главное меню
    menu = new MainMenu(this->menuBar(), this);

    connect(menu, &MainMenu::openFileRequested, this, [this](){
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть конфигурацию", "", "YAML Files (*.yaml *.yml);;All Files (*)");
        if (!filePath.isEmpty()) {
            QString fileName = QFileInfo(filePath).fileName();
            ConfigDockWidget* dock = CreateConfDockWidget(filePath, fileName);

            if (!dock->openFile(filePath)) {
                QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл");
                delete dock;
                return;
            }
            dock->show();
            dock->raise();
            dock->setFocus();
        }
    });

    // 2. Создаем строку состояния (Status Bar)
    QStatusBar *statusBar = this->statusBar();

    // Временное сообщение (исчезнет через 5000 миллисекунд / 5 секунд)
    statusBar->showMessage(tr("Программа готова к работе"), 5000);

    fwWidget = new FirmwareWidget(this);
    statusBar->addPermanentWidget(fwWidget);

    connect(lbplc, &plcManager::firmwareStarted, this, [this]
            (const plcManager::CommandContext &ctx, const QString &message){
                qDebug()<<"plcManager::firmwareStarted"<<ctx.ipv6<<ctx.name;
                this->statusBar()->showMessage(message);
                fwWidget->showStatus();
            });

    connect(lbplc, &plcManager::firmwareProgressChanged,
            fwWidget, &FirmwareWidget::setProgress);

    connect(lbplc, &plcManager::firmwareFinished,
            fwWidget, &FirmwareWidget::resetAndHide);

    connect(treeDock, &DeviceTreeDockWidget::requestUpdate,
            lbplc, &plcManager::scanDevice);
    connect(treeDock, &DeviceTreeDockWidget::requestConfig,
            lbplc, &plcManager::requestConfig);
    connect(lbplc, &plcManager::errorOccurred, this, [this](const QString &msg){
        this->statusBar()->showMessage(msg);
    });
    connect(lbplc, &plcManager::eventOccurred, this, [this](const QString &msg){
        this->statusBar()->showMessage(msg, 5000);
    });
    connect(discoverDock, &DiscoverDockWidget::deviceSelected,
        lbplc, &plcManager::scanDevice);
    connect(discoverDock, &DiscoverDockWidget::requestConfig,
            lbplc, &plcManager::requestConfig);
    connect(lbplc, &plcManager::scanCompleted,
            treeDock, &DeviceTreeDockWidget::updateDevice);
    connect(lbplc, &plcManager::configReceived,
            this, &MainWindow::CreateConfig);

    connect(fwWidget, &FirmwareWidget::stopButtonPressed, this, [this](){
        lbplc->stopFirmware();
    });

}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event); // Обязательно вызываем базу
    // Теперь размеры окна уже реальные (800x600)
    int totalWidth = this->width();

    // Задаем пропорции 1/3 и 2/3
    resizeDocks({treeDock, discoverDock}, {totalWidth/3, 2*totalWidth/3}, Qt::Horizontal);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Проверяем, что событие происходит на панели вкладок
    QTabBar *tabBar = qobject_cast<QTabBar*>(watched);
    if (tabBar && event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent*>(event);
        // Определяем индекс вкладки, на которую указывает курсор
        int index = tabBar->tabAt(helpEvent->pos());
        if (index != -1) {
            QString tabText = tabBar->tabText(index);
            // Ищем документ, соответствующий этой вкладке
            for (ConfigDockWidget *dock : configDocks.values()) {
                if (dock && dock->windowTitle() == tabText) {
                    QString filePath = dock->getCurrentFilePath();
                    if (!filePath.isEmpty()) {
                        // Выводим подсказку на экран в глобальных координатах курсора
                        QToolTip::showText(helpEvent->globalPos(), filePath, tabBar);
                    } else {
                        // Если пути нет, принудительно скрываем подсказку
                        QToolTip::hideText();
                    }
                    return true; // Сообщаем Qt, что событие полностью обработано
                }
            }
        }
        // Если это вкладка "Discover" или любой другой не наш док, скрываем старый текст
        QToolTip::hideText();
    }
    return QMainWindow::eventFilter(watched, event);
}

MainWindow::~MainWindow() {}

ConfigDockWidget *MainWindow::CreateConfDockWidget(const QString &key, const QString &name)
{
    ConfigDockWidget* dock = nullptr;
    if (configDocks.contains(key)) {
        dock = configDocks[key];
    } else {
        dock = new ConfigDockWidget(name, this, lbplc);
        dock->setAttribute(Qt::WA_DeleteOnClose);

        configDocks.insert(key, dock);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        tabifyDockWidget(discoverDock, dock);
        for (QTabBar *tabBar : this->findChildren<QTabBar *>()) {
            tabBar->installEventFilter(this);
        }

        connect(dock, &QObject::destroyed, this, [this, key]() {
            qDebug() << "destroy";
            configDocks.remove(key);
            CommandManager::instance()->resetActiveConfDockWidget();
        });
        connect(lbplc, &plcManager::confCompleted, this, [this](const QString &ipv6, const QString &name){
            if(!(treeDock->containsName(name)))
                lbplc->scanDevice(ipv6, name);
        });
    }
    return dock;
}

void MainWindow::CreateConfig(const QString &ipv6, const QString &name, const QString &content)
{
    ConfigDockWidget* dock = CreateConfDockWidget(ipv6, name);
    dock->setConfig(content);
    dock->show();
    dock->raise();
    // dock->setFocus();
}
