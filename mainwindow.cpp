#include "mainwindow.h"
#include <QDockWidget>
#include <discover.h>
#include <lbclient.h>
#include <lbprocess.h>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>



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


    // 1. Создаем главное меню
    QMenuBar *menuBar = this->menuBar();

    // Добавляем вкладку "Файл"
    QMenu *fileMenu = menuBar->addMenu("&Файл");

    // Добавляем действие "Выход" в меню "Файл"
    QAction *exitAction = fileMenu->addAction("&Выход");
    exitAction->setShortcut(QKeySequence::Quit); // Горячая клавиша

    // Соединяем клик по меню с закрытием программы
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

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
    ConfigDockWidget* dock = nullptr;
    if (configDocks.contains(ipv6))
    {
        dock = configDocks[ipv6];
    }else{
        dock = new ConfigDockWidget(name,this);
        configDocks.insert(ipv6, dock);
        addDockWidget(Qt::RightDockWidgetArea, dock); // Добавляем в ту же область, где ваш основной док (например, dock2)
        tabifyDockWidget(discoverDock, dock); // Превращаем в табы
        connect(dock, &QObject::destroyed, this,
                [this, ipv6](){
                    configDocks.remove(ipv6);
                });
    }
    dock->setConfig(content);
    dock->show();
    dock->raise();
}
