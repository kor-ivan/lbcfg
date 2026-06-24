#include "mainwindow.h"
#include <QDockWidget>
#include <discover.h>
#include <lbclient.h>
#include <lbprocess.h>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileDialog>
#include <QApplication>
#include <QHelpEvent>
#include <QToolTip>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 720);

    QWidget* dummy = new QWidget(this);
    setCentralWidget(dummy);
    dummy->hide(); // Скрываем, чтобы доки сомкнулись в центре

    treeDock = new DeviceTreeDockWidget(this);
    discoverDock = new DiscoverDockWidget(this);
    // Разрешаем прикрепление ко всем сторонам: Left, Right, Top, Bottom
    treeDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    discoverDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Или QWidget, или QMdiArea
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    addDockWidget(Qt::RightDockWidgetArea, discoverDock);
    setDockNestingEnabled(true);


    connect(
        treeDock,
        &DeviceTreeDockWidget::requestConfig,
        this,
        &MainWindow::getlbcfg);

    connect(treeDock, &DeviceTreeDockWidget::requestUpdate,
            this, &MainWindow::onDeviceSelected);

    connect(
        discoverDock,
        &DiscoverDockWidget::deviceSelected,
        this,
        &MainWindow::onDeviceSelected);

    connect(discoverDock, &DiscoverDockWidget::requestConfig,
            this, &MainWindow::getlbcfg);

    connect(discoverDock, &DiscoverDockWidget::newConfig,
            this, [this] (const QString &ipv6, const QString &name){
                CreateConfig(ipv6, name);
            }
            );

    connect(this, &QMainWindow::tabifiedDockWidgetActivated, this, [this](QDockWidget *dock) {
        checkConfigDockWidget(dock);
    });

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *oldFocus, QWidget *newFocus) {
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


    // 1. Создаем главное меню
    QMenuBar *menuBar = this->menuBar();

    // Добавляем вкладку "Файл"
    QMenu *fileMenu = menuBar->addMenu("&Файл");

    // Добавляем действие "Выход" в меню "Файл"
    QAction *exitAction = fileMenu->addAction("&Выход");
    exitAction->setShortcut(QKeySequence::Quit); // Горячая клавиша

    // Соединяем клик по меню с закрытием программы
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QAction *openFile = fileMenu->addAction("Открыть");
    connect(openFile, &QAction::triggered, this, [this](){
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

    saveFileAs = fileMenu->addAction("Сохранить как ...");
    connect(saveFileAs, &QAction::triggered, this, [this](){
        if (!activeConfDockWidget) return;
        activeConfDockWidget->saveFileAs();
    });

    saveFile = fileMenu->addAction("Сохранить");
    connect(saveFile, &QAction::triggered, this, [this](){
        if (!activeConfDockWidget) return;
        activeConfDockWidget->saveFile();
    });

    saveFile->setEnabled(false);
    saveFileAs->setEnabled(false);

    // 2. Создаем строку состояния (Status Bar)
    QStatusBar *statusBar = this->statusBar();

    // Временное сообщение (исчезнет через 5000 миллисекунд / 5 секунд)
    statusBar->showMessage(tr("Программа готова к работе"), 5000);

    // Постоянный индикатор (например, имя пользователя или статус сети)
    QLabel *statusLabel = new QLabel(tr("Сеть: ОК"), this);
    statusBar->addPermanentWidget(statusLabel);

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

void MainWindow::onDeviceSelected(const QString &ipv6, const QString &name)
{
    qDebug() << "Starting process for:"<<ipv6<<" "<<name;
    LBclient *lbc = new LBclient(this);
    lbc->setTCPaddr(ipv6, 502);
    connect(lbc, &LBclient::lbDisconnect, this,
            [lbc](const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                qDebug()<<message<<"disconnect";
                lbc->deleteLater();
            }
            );
    lbprocess *lbproc = new lbprocess(this, lbc);
    connect(lbproc, &lbprocess::outMessage, this,
            [](const QString &lbstr, const QString &message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError)
                    qDebug().noquote()<<lbstr;
                else
                    qDebug().noquote()<<message;
            }
            );
    connect(lbproc, &lbprocess::scanCompleted, this,
            [ipv6, name, lbc, lbproc, this](const QMap<qsizetype, lbprocess::scaninfo>& scan){
                for (auto i = scan.begin(); i != scan.end(); ++i) {
                    qDebug()<<i.key()<<i.value();
                }
                treeDock->updateDevice(ipv6, name, scan);
                lbproc->deleteLater();
                lbc->deleteLater();
            }
            );
    lbproc->run(lbprocess::scan, {"sys.serial"});
}

MainWindow::~MainWindow() {}

// bool MainWindow::isConfigDockWidget()
// {
//     if (!activeConfDockWidget) {
//         QMessageBox::warning(this,
//                              "Внимание",
//                              "Не выбрано активное окно конфигурации.");
//         return false;
//     }
//     return true;
// }

void MainWindow::checkConfigDockWidget(QDockWidget *dock)
{
    // qDebug()<< "Фокус на Dock:" <<dock;
    ConfigDockWidget *configDock = qobject_cast<ConfigDockWidget*>(dock);
    if (configDock) {
        qDebug() << "Выбран ConfigDockWidget: " << configDock;
        activeConfDockWidget = configDock;
        QString plcName = activeConfDockWidget->getPlcName();
        if (saveFile){
            saveFile->setEnabled(true);
            saveFile->setText(QString("Сохранить %1").arg(plcName));
        }
        if (saveFileAs) {
            saveFileAs->setEnabled(true);
            saveFileAs->setText(QString("Сохранить %1 как ...").arg(plcName));
        }

        // return;
    }
    // if (!(activeConfDockWidget && activeConfDockWidget->isFloating())) {
    //     activeConfDockWidget = nullptr;
    //     qDebug() << "Активный виджет сброшен.";
    // }
}

// void MainWindow::SaveConfigAs(ConfigDockWidget* activeDock)
// {
//     // qDebug()<<activeDock->getPlcName();

//     // qDebug()<<fileName;

// }

ConfigDockWidget *MainWindow::CreateConfDockWidget(const QString &key, const QString &name)
{
    ConfigDockWidget* dock = nullptr;
    if (configDocks.contains(key)) {
        dock = configDocks[key];
    } else {
        dock = new ConfigDockWidget(name, treeDock, this);
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
            activeConfDockWidget = nullptr;
            saveFile->setEnabled(false);
            saveFile->setText("Сохранить");
            saveFileAs->setEnabled(false);
            saveFileAs->setText("Сохранить как ...");
        });
        // connect(dock, &ConfigDockWidget::getSaveFile, this, &MainWindow::onSaveFileTriggered);
        connect(dock, &ConfigDockWidget::updateScan, this, &MainWindow::onDeviceSelected);
    }

    return dock;
}

// ConfigDockWidget* MainWindow::findActiveConfigDockWidget()
// {
//     // QList<ConfigDockWidget*> allDocks = this->findChildren<ConfigDockWidget*>();
//     // for (ConfigDockWidget *dock : allDocks) {
//     //     // Из всех объединенных доков видимым (isVisible) будет только тот,
//     //     // вкладку которого пользователь выбрал на экране
//     //     // if (dock->isVisible())
//     //     //     return dock;
//     //     qDebug()<<dock->getPlcName();
//     //     qDebug()<<dock->isVisible();
//     //     qDebug()<<dock->isActiveWindow();
//     // }
//     // return nullptr;
// }

void MainWindow::getlbcfg(const QString &ipv6, const QString &name)
{
    qDebug()<<"getlbcfg: "<<ipv6<<name;
    LBclient *lbc = new LBclient(this, {"getconf"});
    lbc->setTCPaddr(ipv6, 502);
    connect(lbc, &LBclient::ExecuteCompletedJson, this,
            [lbc, this, name, ipv6](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    qDebug()<<"# BEGIN YAML";
                    lbyaml::printlbconf(Qjo);
                    qDebug()<<"# END YAML";
                    // Получаем YAML-текст один раз, чтобы использовать его для сравнения
                    QString yamlContent = lbyaml::getlbconf(Qjo);
                    CreateConfig(ipv6, name, yamlContent);
                }
                else
                    qDebug().noquote()<<message;
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}

void MainWindow::CreateConfig(const QString &ipv6, const QString &name, const QString &content)
{
    ConfigDockWidget* dock = CreateConfDockWidget(ipv6, name);
    dock->setConfig(content);
    dock->show();
    dock->raise();
    // dock->setFocus();
}

// void MainWindow::onSaveFileTriggered()
// {
//     if (!activeConfDockWidget) return;
//     if (activeConfDockWidget->getCurrentFilePath().isEmpty()) {
//         SaveConfigAs(activeConfDockWidget);
//     } else {
//         qDebug()<<activeConfDockWidget->saveFile();
//     }
// }
