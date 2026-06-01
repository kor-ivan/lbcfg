#include "mainwindow.h"
#include <QDockWidget>
#include <QLayout>
#include <QHeaderView>
#include <discover.h>
#include <lbclient.h>
#include <lbprocess.h>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include "configwidget.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 720); // Ширина 800, Высота 600

    QWidget* dummy = new QWidget(this);
    setCentralWidget(dummy);
    dummy->hide(); // Скрываем, чтобы доки сомкнулись в центре

    dock1 = new QDockWidget("Tree View", this);
    dock2 = new QDockWidget("Discover", this);
    // Разрешаем прикрепление ко всем сторонам: Left, Right, Top, Bottom
    dock1->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock2->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Или QWidget, или QMdiArea
    addDockWidget(Qt::LeftDockWidgetArea, dock1);
    addDockWidget(Qt::RightDockWidgetArea, dock2);
    setDockNestingEnabled(true);


    treeWidget = new DeviceTreeWidget(this);
    dock1->setWidget(treeWidget);

    connect(
        treeWidget,
        &DeviceTreeWidget::requestConfig,
        this,
        &MainWindow::getlbcfg);


    m_discoverWidget = new DiscoverWidget(this);
    dock2->setWidget(m_discoverWidget);

    connect(
        m_discoverWidget,
        &DiscoverWidget::deviceSelected,
        this,
        &MainWindow::onDeviceSelected);


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
    resizeDocks({dock1, dock2}, {totalWidth/3, 2*totalWidth/3}, Qt::Horizontal);
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
                auto i = scan.begin();
                for (auto i = scan.begin(); i != scan.end(); ++i) {
                    qDebug()<<i.key()<<i.value();
                }

                treeWidget->updateDevice(ipv6, name, scan);

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

                    QDockWidget* dock = nullptr;
                    ConfigWidget* cfgWidget = nullptr;

                    if (configDocks.contains(ipv6))
                    {
                        dock = configDocks[ipv6];

                        cfgWidget = qobject_cast<ConfigWidget*>(dock->widget());
                    }else{
                        dock = new QDockWidget(QString("Конфигурация: %1").arg(name), this);
                        cfgWidget = new ConfigWidget();
                        dock->setWidget(cfgWidget);
                        configDocks.insert(ipv6, dock);
                        // Добавляем в ту же область, где ваш основной док (например, dock2)
                        addDockWidget(Qt::RightDockWidgetArea, dock);
                        // Превращаем в табы
                        tabifyDockWidget(dock2, dock);
                        cfgWidget->setConfig(yamlContent);
                    }
                    connect(dock, &QObject::destroyed, this,
                        [this, ipv6](){
                            configDocks.remove(ipv6);
                        });
                    connect(
                        cfgWidget, &ConfigWidget::modifiedChanged, this,
                        [dock, name](bool modified)
                        {
                            if(modified)
                            {
                                dock->setWindowTitle(
                                    QString("* Конфигурация: %1").arg(name));
                            }
                            else
                            {
                                dock->setWindowTitle(
                                    QString("Конфигурация: %1").arg(name));
                            }
                        });
                    dock->show();
                    dock->raise();
                }
                else
                    qDebug().noquote()<<message;
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}
