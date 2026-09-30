#include "mainwindow.h"
#include "version.h"
#include <QLabel>
#include <QDockWidget>
#include <QStatusBar>
#include <QFileDialog>
#include <QApplication>
#include <QHelpEvent>
#include <QToolTip>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDir>
#include "firmwarewidget.h"
#include "devicetreedockwidget.h"
#include "discoverdockwidget.h"
#include "configdockwidget.h"
#include "plcmanager.h"
#include "mainmenu.h"
#include "logdockwidget.h"
#include "watchdockwidget.h"
#include "commandmanager.h"
#include "firmwarerepositorydialog.h"
#include "appsettings.h"

// === КЛАСС РЕАЛИЗАЦИИ (PIMPL) ===
class MainWindowPrivate {
public:
    MainWindow *q_ptr;
    MainWindowPrivate(MainWindow *q) : q_ptr(q) {}

    plcManager *lbplc = nullptr;
    QPointer<DeviceTreeDockWidget> treeDock = nullptr;
    QPointer<DiscoverDockWidget> discoverDock = nullptr;
    QPointer<LogDockWidget> logDock = nullptr;
    QMap<QString, ConfigDockWidget*> configDocks;
    QMap<QString, WatchDockWidget*> watchDocks;
    MainMenu *menu = nullptr;
    FirmwareWidget *fwWidget = nullptr;
    firmwareAnalyzer *repo = nullptr;
    QMetaObject::Connection scanConnection;

    ConfigDockWidget* CreateConfDockWidget(const QString &key, const QString &name);
    void CreateConfig(const plc::CommandContext &ctx, const QString &content = {});
    QList<QDockWidget*> getDocksInArea(Qt::DockWidgetArea area) const;
    void tabifyDockWidgetTo(QDockWidget *dock, Qt::DockWidgetArea area);
    void checkTreeAndStartScan(const plc::CommandContext &ctx);
    void setupStatusLabel(QLabel* label, bool active, const QString& text, const QString& tooltip);

};

// === РЕАЛИЗАЦИЯ ПРИВАТНЫХ МЕТОДОВ (из MainWindowPrivate) ===
ConfigDockWidget *MainWindowPrivate::CreateConfDockWidget(const QString &key, const QString &name)
{
    ConfigDockWidget* dock = nullptr;
    if (configDocks.contains(key)) {
        dock = configDocks[key];
    } else {
        dock = new ConfigDockWidget(name, q_ptr);
        dock->setAttribute(Qt::WA_DeleteOnClose);

        configDocks.insert(key, dock);
        tabifyDockWidgetTo(dock, Qt::RightDockWidgetArea);
        for (QTabBar *tabBar : q_ptr->findChildren<QTabBar *>()) {
            tabBar->installEventFilter(q_ptr);
        }

        QObject::connect(dock, &QObject::destroyed, q_ptr, [this, key]() {
            debugApp() << "destroy ConfDockWidget: "<<key;
            configDocks.remove(key);
            CommandManager::instance()->resetActiveConfDockWidget();
            // Если это был последний док, разрываем соединение
            if (configDocks.isEmpty() && scanConnection) {
                QObject::disconnect(scanConnection);
                scanConnection = QMetaObject::Connection();
                qDebug() << "scanConnection = QMetaObject::Connection()";
            }
        });
        if (!scanConnection) {
            scanConnection = QObject::connect(lbplc, &plcManager::confCompleted, q_ptr, [this](const plc::CommandContext &ctx){
                this->checkTreeAndStartScan(ctx);
            });
        }
    }
    return dock;
}

void MainWindowPrivate::CreateConfig(const plc::CommandContext &ctx, const QString &content)
{
    ConfigDockWidget* dock = CreateConfDockWidget(ctx.ipv6str(), ctx.name);
    dock->setConfig(content);
    dock->show();
    dock->raise();
    // dock->setFocus();
}

QList<QDockWidget *> MainWindowPrivate::getDocksInArea(Qt::DockWidgetArea area) const
{
    QList<QDockWidget*> result;

    // 1. Находим вообще все QDockWidget, принадлежащие главному окну
    QList<QDockWidget*> allDocks = q_ptr->findChildren<QDockWidget*>();

    // 2. Фильтруем их по текущей области
    for (QDockWidget *dock : allDocks) {
        if (dock && q_ptr->dockWidgetArea(dock) == area) {
            result.append(dock);
        }
    }

    return result;
}

void MainWindowPrivate::tabifyDockWidgetTo(QDockWidget *dock, Qt::DockWidgetArea area)
{
    QList<QDockWidget*> areaDocks = getDocksInArea(area);

    QDockWidget* targetForTab = nullptr;
    for (QDockWidget* d : areaDocks) {
        if (d->isVisible()) {
            targetForTab = d;
            break;
        }
    }

    if (targetForTab) {
        q_ptr->tabifyDockWidget(targetForTab, dock);
    } else {
        q_ptr->addDockWidget(area, dock);
    }
}

void MainWindowPrivate::checkTreeAndStartScan(const plc::CommandContext &ctx)
{
    if(!(treeDock->containsName(ctx.name)))
        lbplc->scanDevice(ctx);
}

void MainWindowPrivate::setupStatusLabel(QLabel* label, bool active, const QString& text, const QString& tooltip) {
    label->setText(text);
    label->setToolTip(tooltip);

    if (active) {
        label->setStyleSheet(
            "QLabel {"
            "  background-color: #D4EDDA;"
            "  color: #155724;"
            "  border: 1px solid #C3E6CB;"
            "  padding: 0px 6px;"
            "  border-radius: 3px;"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "}"
            );
    } else {
        label->setStyleSheet(
            "QLabel {"
            "  padding: 2px 8px;"
            "  border-radius: 3px;"
            "  color: #6c757d;"
            "  background-color: transparent;" // Явно сбрасываем фон и рамку
            "  border: none;"
            "}"
            );
    }
}

// === РЕАЛИЗАЦИЯ КЛАССА MAINWINDOW (ОСНОВНОЙ ИНТЕРФЕЙС) ===

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    p(new MainWindowPrivate(this))
{
    p->repo = new firmwareAnalyzer(this, AppSettings::firmwareRepositoryRoot());

    resize(1280, 720);

    QWidget* dummy = new QWidget(this);
    setCentralWidget(dummy);
    dummy->hide(); // Скрываем, чтобы доки сомкнулись в центре

    CommandManager::instance()->setFirmwareAnalyzer(p->repo);

    p->lbplc = plcManager::instanse();
    createTreeDockWidget();
    createDiscoverDockWidget();
    setDockNestingEnabled(true);
    connect(this, &QMainWindow::tabifiedDockWidgetActivated,
            CommandManager::instance(), &CommandManager::checkDockWidget);
    createLogDockWidget();

    // 1. Создаем главное меню
    p->menu = new MainMenu(this);

    connect(p->menu, &MainMenu::openFileRequested, this, [this](){
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть конфигурацию", "", "YAML Files (*.yaml *.yml);;All Files (*)");
        if (!filePath.isEmpty()) {
            QString fileName = QFileInfo(filePath).fileName();
            ConfigDockWidget* dock = p->CreateConfDockWidget(filePath, fileName);

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

    connect(p->menu, &MainMenu::newConfigurationRequested, this, [this]{
        ConfigDockWidget* dock = p->CreateConfDockWidget(QUuid::createUuid().toString(), "noname");
        dock->show();
        dock->raise();
        dock->setFocus();
    });
    // Создаем строку состояния (Status Bar)
    QStatusBar *statusBar = this->statusBar();

    // Временное сообщение (исчезнет через 5000 миллисекунд / 5 секунд)
    statusBar->showMessage(tr("Программа готова к работе"), 5000);


    QLabel *repStatusLabel = new QLabel(this);
    repStatusLabel->setAlignment(Qt::AlignCenter);
    repStatusLabel->setFixedHeight(20);
    statusBar->addPermanentWidget(repStatusLabel);

    connect(p->repo, &firmwareAnalyzer::updated, this, [this, repStatusLabel](const QString &hash) {
        bool hasHash = !hash.isEmpty();
        QString text = hasHash ? QString("logicbox: %1").arg(hash) : "logicbox: Not found";
        QString tip = hasHash ? p->repo->path() : "Репозиторий прошивок не настроен";

        p->setupStatusLabel(repStatusLabel, hasHash, text, tip);
    });
    p->repo->update();

    QLabel *watchStatusLabel = new QLabel(this);
    watchStatusLabel->setAlignment(Qt::AlignCenter);
    watchStatusLabel->setFixedHeight(20);
    statusBar->addPermanentWidget(watchStatusLabel);

    connect(p->lbplc, &plcManager::activeWatchChanged, this, [this, watchStatusLabel](const QStringList &keys) {
        int count = keys.size();
        bool hasConnections = count > 0;
        QString text = hasConnections ? QString("Connected: %1").arg(count) : "No Connections";
        QString tip = hasConnections ? keys.join("\n") : "No active watch lists";

        p->setupStatusLabel(watchStatusLabel, hasConnections, text, tip);
    });
    emit p->lbplc->activeWatchChanged(p->lbplc->activeWatchKeys());


    p->fwWidget = new FirmwareWidget(this);
    statusBar->addPermanentWidget(p->fwWidget);


    connect(p->lbplc, &plcManager::firmwareStarted, this, [this]
            (const plc::CommandContext &ctx, const QString &message){
                debugApp()<<"plcManager::firmwareStarted"<<ctx.ipv6str()<<ctx.name;
                this->statusBar()->showMessage(message);
                p->fwWidget->showStatus();
            });
    connect(p->lbplc, &plcManager::firmwareProgressChanged,
            p->fwWidget, &FirmwareWidget::setProgress);

    connect(p->lbplc, &plcManager::firmwareFinished,
            p->fwWidget, &FirmwareWidget::resetAndHide);

    connect(p->lbplc, &plcManager::errorOccurred, this, [this](const QString &msg){
        this->statusBar()->showMessage(msg);
    });
    connect(p->lbplc, &plcManager::eventOccurred, this, [this](const QString &msg){
        this->statusBar()->showMessage(msg, 5000);
    });
    QObject::connect(p->lbplc, &plcManager::configReceived, this, [this](const plc::CommandContext &ctx, const QString &content) {
        p->CreateConfig(ctx, content);
    });

    connect(p->fwWidget, &FirmwareWidget::stopButtonPressed, this, [this](){
        debugApp() << "Stop firmware button pressed";
        p->lbplc->stopFirmware();
    });

    connect(p->lbplc, &plcManager::logStarted, this, &MainWindow::createLogDockWidget);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event); // Обязательно вызываем базу
    // Теперь размеры окна уже реальные (800x600)
    int totalWidth = this->width();

    // Задаем пропорции 1/3 и 2/3
    resizeDocks({p->treeDock, p->discoverDock}, {totalWidth/3, 2*totalWidth/3}, Qt::Horizontal);

    int totalHeight = this->height();
    if (p->discoverDock && p->logDock) {
        resizeDocks({p->discoverDock.get(), p->logDock.get()}, {totalHeight - 150, 150}, Qt::Vertical);
    }
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
            for (ConfigDockWidget *dock : std::as_const(p->configDocks)) {
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

QPointer<DeviceTreeDockWidget> MainWindow::getTreeDock() const
{
    return p->treeDock.get();
}

QPointer<DiscoverDockWidget> MainWindow::getDiscoverDock() const
{
    return p->discoverDock.get();
}



DeviceTreeDockWidget *MainWindow::createTreeDockWidget()
{
    if (p->treeDock)
        return p->treeDock.get();
    p->treeDock = new DeviceTreeDockWidget(this);
    p->treeDock->setAttribute(Qt::WA_DeleteOnClose); // Чтобы док уничтожался при нажатии на крестик
    p->treeDock->setWindowTitle("Device Tree");
    p->treeDock->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(CommandManager::instance(), &CommandManager::requestFlash, this, [this]
            (const plc::CommandContext &ctx){
                QString filePath = QFileDialog::getOpenFileName(
                    this,
                    "Загрузить прошивку ...",
                    "",
                    "LogicBox Firmware (*.bin *.bin.xz *.xz);;BIN Files (*.bin);;XZ compressed (*.xz);;All Files (*)");
                if (!filePath.isEmpty()) {
                    p->lbplc->startFirmware(ctx, filePath,
                                         "Загрузка уже выполняется, дождитесь окончания",
                                         QString("Загрузка прошивки в %1 ...").arg(ctx.displayName()));
                }
            });
    connect(p->treeDock, &DeviceTreeDockWidget::requestFlashAll, this, [this]
            (const plc::CommandContext &ctx, const QStringList &otaSlots){
        QString filePath;
        if (otaSlots.isEmpty())
                filePath = QFileDialog::getExistingDirectory(this, "Выберите директорию для прошивки ...", "", QFileDialog::DontResolveSymlinks);
        else
            filePath = AppSettings::firmwareRepositoryRoot();
                if (!filePath.isEmpty()) {
                    p->lbplc->startFirmwareAll(ctx, filePath,
                                            "Загрузка уже выполняется, дождитесь окончания",
                                            QString("Загрузка прошивки в %1 ...").arg(ctx.displayName()), otaSlots);
                }
            });
    connect(p->treeDock, &DeviceTreeDockWidget::requestFboot, this, [this]
            (const plc::CommandContext &ctx){
                QString filePath = QFileDialog::getOpenFileName(this, "Загрузить fboot ...", "", "Fboot Files (*.fboot);;All Files (*)");                if (!filePath.isEmpty()) {
                    p->lbplc->startFirmware(ctx, filePath,
                                         "Загрузка уже выполняется, дождитесь окончания",
                                         QString("Загрузка fboot в %1 ...").arg(ctx.displayName()),
                                         "fboot");
                }
            });
    connect(p->treeDock, &DeviceTreeDockWidget::requestUpdate,
            p->lbplc, &plcManager::scanDevice);
    connect(p->treeDock, &DeviceTreeDockWidget::requestConfig,
            p->lbplc, &plcManager::requestConfig);

    connect(p->lbplc, &plcManager::scanCompleted, this, [this]
            (const plc::CommandContext &ctx, const QMap<qsizetype, lbprocess::scaninfo> &scanData){
        if (!p->treeDock)
            createTreeDockWidget();
        // qDebug() << "into scanCompleted.." << ctx.ipv6;
        p->treeDock->updateDevice(ctx, scanData);
        p->treeDock->show();
        p->treeDock->raise();
        p->treeDock->setFocus();
    });

    p->tabifyDockWidgetTo(p->treeDock, Qt::LeftDockWidgetArea);

    return p->treeDock.get();
}

DiscoverDockWidget *MainWindow::createDiscoverDockWidget()
{
    if (p->discoverDock)
        return p->discoverDock.get();
    p->discoverDock = new DiscoverDockWidget(this);
    p->discoverDock->setAttribute(Qt::WA_DeleteOnClose);
    p->discoverDock->setWindowTitle("Discover");
    p->discoverDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    connect(p->discoverDock, &DiscoverDockWidget::newConfig,
            this, [this] (const plc::CommandContext &ctx){
                p->CreateConfig(ctx);
            }
            );
    connect(p->discoverDock, &DiscoverDockWidget::deviceSelected,
            p->lbplc, &plcManager::scanDevice);
    connect(p->discoverDock, &DiscoverDockWidget::requestConfig,
            p->lbplc, &plcManager::requestConfig);

    p->tabifyDockWidgetTo(p->discoverDock, Qt::RightDockWidgetArea);
    return p->discoverDock.get();
}

LogDockWidget *MainWindow::createLogDockWidget()
{
    if (p->logDock) return p->logDock.get();

    p->logDock = new LogDockWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, p->logDock.get());

    if (p->getDocksInArea(Qt::RightDockWidgetArea).isEmpty()) {
        splitDockWidget(p->discoverDock.get(), p->logDock.get(), Qt::Vertical);
    }
    p->logDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    connect(p->lbplc, &plcManager::logStarted, p->logDock, &LogDockWidget::onLogStarted);
    connect(p->lbplc, &plcManager::logFinished, p->logDock, &LogDockWidget::onLogFinished);
    connect(p->logDock, &LogDockWidget::stopButtonPressed, p->lbplc, &plcManager::stopLog);
    connect(p->logDock, &QObject::destroyed, p->lbplc, [this](){
        disconnect(p->lbplc, &plcManager::logFinished, p->logDock, &LogDockWidget::onLogFinished);
        p->lbplc->stopLog();
    });

    return p->logDock.get();
}

WatchDockWidget *MainWindow::createWatchDockWidget(const plc::CommandContext &ctx)
{
    WatchDockWidget* dock = nullptr;
    if (p->watchDocks.contains(ctx.name))
        dock = p->watchDocks[ctx.name];
    else{
        dock = new WatchDockWidget(ctx, this);
        dock->setAttribute(Qt::WA_DeleteOnClose);
        // if (!ctx.ipv6str().isEmpty())
        //     dock->setIpv6(ipv6);
        p->watchDocks.insert(ctx.name, dock);
        p->tabifyDockWidgetTo(dock, Qt::LeftDockWidgetArea);

        connect(dock, &QObject::destroyed, this, [this, ctx]() {
            debugApp() << "destroy WatchDockWidget: "<< ctx.name;
            p->watchDocks.remove(ctx.name);
        });
    }
    return dock;
}

void MainWindow::editFirmwareRepositorySettings()
{
    FirmwareRepositoryDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted)
        return;
    // CommandManager::instance()->getFirmwareAnalyzer()->update();

    const QString repositoryRoot =
        dialog.repositoryPath();

    p->repo->setPath(repositoryRoot);

    p->repo->update();

    if (repositoryRoot.isEmpty()) {
        if (p->treeDock) {
            p->treeDock->reloadFirmwareRepositoryFromSettings();
        }

        statusBar()->showMessage(
            QStringLiteral("Путь к репозиторию прошивок сброшен"),
            5000);

        return;
    }

    if (p->treeDock) {
        p->treeDock->reloadFirmwareRepositoryFromSettings();
    }

    statusBar()->showMessage(
        QStringLiteral("Репозиторий прошивок: %1")
            .arg(QDir::toNativeSeparators(repositoryRoot)),
        5000);
}

QString MainWindow::getLibraryVersion()
{
    return QStringLiteral(APP_VERSION_STRING);
}

QList<ConfigDockWidget *> MainWindow::getConfigDocks() const
{
    return p->configDocks.values();
}

QList<WatchDockWidget *> MainWindow::getWatchDocks() const
{
    return p->watchDocks.values();
}

QPointer<LogDockWidget> MainWindow::getLogDock() const
{
    return p->logDock;
}
