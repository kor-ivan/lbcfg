#include "mainwindow.h"
#include <QDockWidget>
#include <QLayout>
#include <QHeaderView>
#include <QPushButton>
#include <discover.h>
#include <QApplication>
#include <lbclient.h>
#include <lbprocess.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 720); // Ширина 800, Высота 600

    QWidget* dummy = new QWidget(this);
    setCentralWidget(dummy);
    dummy->hide(); // Скрываем, чтобы доки сомкнулись в центре

    dock1 = new QDockWidget(tr("Tree View"), this);
    dock2 = new QDockWidget(tr("Discover"), this);
    // Разрешаем прикрепление ко всем сторонам: Left, Right, Top, Bottom
    dock1->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock2->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Или QWidget, или QMdiArea
    addDockWidget(Qt::LeftDockWidgetArea, dock1);
    addDockWidget(Qt::RightDockWidgetArea, dock2);

    setDockNestingEnabled(true);


    QWidget *dock1Content = new QWidget();
    QVBoxLayout *layout1 = new QVBoxLayout(dock1Content);

    treeView = new QTreeView();
    treeModel = new QStandardItemModel(this);

    treeView->setModel(treeModel);
    treeView->setHeaderHidden(true);
    // treeView->header()->setSectionResizeMode(QHeaderView::Stretch);

    layout1->addWidget(treeView);
    dock1->setWidget(dock1Content);


    QWidget *wgtDiscover = new QWidget();
    QVBoxLayout *vbox = new QVBoxLayout(wgtDiscover);
    // 2. Создаем кнопку
    QPushButton *btnDiscover = new QPushButton("Send Discover");
    // 1. Создаем горизонтальный слой для верхней части
    QHBoxLayout *hBox = new QHBoxLayout();
    // 2. Добавляем кнопку
    hBox->addWidget(btnDiscover);
    // Соединяем: КТО (кнопка) -> ЧТО СДЕЛАЛА (нажата) -> КТО ПРИМЕТ (окно) -> ЧТО СДЕЛАЕТ (метод)
    connect(btnDiscover, &QPushButton::clicked, this, &MainWindow::btnDiscoverClicked);
    // 3. Добавляем "пружину" (Spacer), которая вытолкнет кнопку влево
    hBox->addStretch();
    // 4. Добавляем этот горизонтальный слой в основной вертикальный
    vbox->addLayout(hBox);

    // 3. Создаем таблицу
    table = new QTableWidget();
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({"Name", "Type", "IPv4", "MAC", "Delays (ms)", "IF"});
    // Выделять строку целиком при клике на любую ячейку
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // разрешить выбирать только одну строку за раз
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    // запретить редактирование ячеек, чтобы они не открывались по двойному клику
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    // Остальные столбцы: подгоняем под содержимое текста
    for (int i = 1; i < table->columnCount(); ++i) {
        header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    header->setStyleSheet(
        "QHeaderView::section {"
        "    background-color: #f0f0f0;"
        "    font-weight: bold;"
        "    border: 1px solid #dcdcdc;"
        "    padding-left: 4px;" // Отступ только слева, чтобы текст не прилипал
        "    height: 20px;"      // Подсказка для высоты
        "}"
        );
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    table->setColumnHidden(6, true);
    connect(table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableDoubleClicked);
    vbox->addWidget(table);
    // vbox->addStretch(1);
    dock2->setWidget(wgtDiscover);
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

void MainWindow::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (QApplication::mouseButtons() != Qt::LeftButton) {
        return;
    }
    QTableWidgetItem *item = table->item(row, 6);
    if (!item) return;
    QString ipv6 = item->text();
    item = table->item(row, 0);
    QString name = item->text();
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
            [ipv6, name, lbproc, this](const QMap<qsizetype, lbprocess::scaninfo>& scan){
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
                }
                // 3. Раскрываем дерево
                treeView->expand(rootIndex);

                lbproc->deleteLater();
            }
            );
    lbproc->run(lbprocess::scan);
}

MainWindow::~MainWindow() {}
