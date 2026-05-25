#include "mainwindow.h"
#include <QDockWidget>
#include <QLayout>
#include <QHeaderView>
#include <QPushButton>
#include <discover.h>
#include <QApplication>
#include <lbclient.h>
#include <lbprocess.h>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include "discoverwidget.h"

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


    discoverWidget *m_discoverWidget = new discoverWidget(this);

    dock2->setWidget(m_discoverWidget);

    connect(
        m_discoverWidget,
        &discoverWidget::deviceSelected,
        this,
        &MainWindow::onDeviceSelected);


    // dockConfig = new QDockWidget(tr("Конфигурация"), this);
    // configDisplay = new QTextEdit(this);
    // configDisplay->setReadOnly(true); // Только для чтения
    // dockConfig->setWidget(configDisplay);

    // // Добавляем его в ту же область, где лежит discover (допустим, это dock2)
    // addDockWidget(Qt::RightDockWidgetArea, dockConfig);

    // // ТАБИФИКАЦИЯ: Эта команда превратит два дока во вкладки
    // tabifyDockWidget(dock2, dockConfig);

    // // По умолчанию показываем discover
    // dock2->raise();

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

void MainWindow::btnDiscoverClicked()
{
    discover *lbd = new discover(this);
    // table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setRowCount(0); // Очищаем старые строки
    connect(lbd, &discover::discoverCompleted, this,
            [this, lbd] (const QMap<QString, discover::lbinfo>& DiscoverMap, const discover::discoverError error, const QString errorStr){
                if (SendDiscover && error == discover::NoError){
                    table->setSortingEnabled(false); // Отключаем сортировку на время вставки для скорости
                    int row = 0;
                    for (auto it = DiscoverMap.begin(); it != DiscoverMap.end(); ++it) {
                        qDebug()<<it.value();
                        table->insertRow(row);
                        table->setItem(row, 0, new QTableWidgetItem(it.value().name));
                        table->setItem(row, 1, new QTableWidgetItem(it.value().type));
                        table->setItem(row, 2, new QTableWidgetItem(it.value().ipv4));
                        table->setItem(row, 3, new QTableWidgetItem(it.value().mac));
                        QStringList strList;
                        for (float val : it.value().delay)
                            strList << QString::number(val);
                        table->setItem(row, 4, new QTableWidgetItem(strList.join(",")));
                        strList.clear();
                        for (int val : it.value().ifindex)
                            strList << QString::number(val);
                        table->setItem(row, 5, new QTableWidgetItem(strList.join(",")));
                        if (it.value().btn) {
                            for (int col = 0; col < table->columnCount(); ++col) {
                                QTableWidgetItem *item = table->item(row, col);
                                if (item) {
                                    item->setBackground(alertColor);
                                    QFont font = item->font();
                                    font.setBold(true);
                                    item->setFont(font);
                                }
                            }
                        }
                        table->setItem(row, 6, new QTableWidgetItem(it.key()));
                        row++;
                    }
                    table->setSortingEnabled(true); // Возвращаем возможность сортировки
                }
                SendDiscover = false;
                lbd->deleteLater();
            }
            );
    if (!SendDiscover){
        lbd->execute();
        SendDiscover = true;
    }
}


void MainWindow::showTreeContextMenu(const QPoint &pos)
{
    // Получаем индекс элемента, на который кликнули
    QModelIndex index = treeView->indexAt(pos);
    if (!index.isValid() || index.parent().isValid()) return;


    QMenu menu(this);
    QAction *removeAction = menu.addAction(QString("Удалить %1").arg(index.data().toString()));
    QAction *getConfigAction = menu.addAction(QString("Запросить конфигурацию у %1").arg(index.data().toString()));

    // Вызываем меню в позиции курсора
    QAction *selectedItem = menu.exec(treeView->viewport()->mapToGlobal(pos));

    if (selectedItem == removeAction) {
        // Удаляем строку из модели
        treeModel->removeRow(index.row());
    }else if (selectedItem == getConfigAction){
        getlbcfg(index.data(Qt::UserRole).toString(), index.data().toString());
    }
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

                // 1. Ищем существующий узел (например, через UserRole с IPv6 или по имени)
                QStandardItem *plcRoot = nullptr;
                qDebug()<<treeModel->rowCount();
                for (int i = 0; i < treeModel->rowCount(); ++i) {
                    if (treeModel->item(i)->data(Qt::UserRole).toString() == ipv6) {
                        plcRoot = treeModel->item(i);
                        plcRoot->removeRows(0, plcRoot->rowCount());
                        break;
                    }
                }


                // 2. Если не нашли, создаем новый корень
                QModelIndex rootIndex;
                if (!plcRoot) {
                    plcRoot = new QStandardItem(name);
                    plcRoot->setData(ipv6, Qt::UserRole); // Прячем ID для поиска в будущем
                    QFont rootFont = plcRoot->font();
                    rootFont.setBold(true);
                    rootFont.setPointSize(rootFont.pointSize());
                    plcRoot->setFont(rootFont);
                    treeModel->appendRow(plcRoot);
                    rootIndex = treeModel->index(treeModel->rowCount() - 1, 0);
                }

                // 3. Итерируем по результатам сканирования
                for (auto it = scan.begin(); it != scan.end(); ++it) {
                    const auto &info = it.value();
                    // Создаем элементы для двух колонок
                    QStandardItem *col1 = new QStandardItem(QString("Slot %1: %2").arg(it.key()).arg(info.devtype));
                    // 2. Делаем их жирными
                    QFont boldFont = col1->font();
                    boldFont.setBold(true);
                    col1->setFont(boldFont);
                    if (info.master)
                        col1->setText(col1->text() + " [MASTER]");
                    // Добавляем их в модель как одну строку
                    plcRoot->appendRow(col1);
                    // Теперь добавляем подробности ВНУТРЬ (как подветки)
                    col1->appendRow(new QStandardItem("MAC: " + info.mac));
                    col1->appendRow(new QStandardItem("Version: " + info.version));
                    col1->appendRow(new QStandardItem("Serial: " + info.data.at(0)));
                }
                // 3. Раскрываем дерево
                treeView->expand(rootIndex);

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
            [lbc, this, name](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    qDebug()<<"# BEGIN YAML";
                    lbyaml::printlbconf(Qjo);
                    qDebug()<<"# END YAML";

                    // Получаем YAML-текст один раз, чтобы использовать его для сравнения
                    QString yamlContent = lbyaml::getlbconf(Qjo);

                    // 1. Проверяем, создан ли док. Если нет — создаем.
                    if (!dockConfig) {
                        dockConfig = new QDockWidget(QString("Конфигурация: %1").arg(name), this);
                        configDisplay = new QTextEdit(this);
                        // configDisplay->setReadOnly(true);
                        configDisplay->setFontFamily("Courier New"); // Моноширинный шрифт для конфига
                        dockConfig->setWidget(configDisplay);

                        // Добавляем в ту же область, где ваш основной док (например, dock2)
                        addDockWidget(Qt::RightDockWidgetArea, dockConfig);

                        // Превращаем в табы
                        tabifyDockWidget(dock2, dockConfig);
                    }

                    // Очищаем старые соединения, если док переиспользуется для нового запроса
                    configDisplay->disconnect(SIGNAL(textChanged()));

                    // 2. Обновляем содержимое
                    dockConfig->setWindowTitle(QString("Конфигурация: %1").arg(name));
                    configDisplay->setPlainText(yamlContent);

                    // 3. Отслеживаем изменения текста (Лямбда-слот)
                    // Захватываем yamlContent (исходный текст) и name по значению
                    connect(configDisplay, &QTextEdit::textChanged, this, [this, yamlContent, name]() {
                        // Если текущий текст отличается от оригинального
                        if (configDisplay->toPlainText() != yamlContent) {
                            dockConfig->setWindowTitle(QString("* Конфигурация: %1").arg(name));
                        } else {
                            // Если пользователь вернул всё как было, убираем звёздочку
                            dockConfig->setWindowTitle(QString("Конфигурация: %1").arg(name));
                        }
                    });

                    // 3. Выводим на передний план
                    dockConfig->show();
                    dockConfig->raise();
                }
                else
                    qDebug().noquote()<<message;
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}
