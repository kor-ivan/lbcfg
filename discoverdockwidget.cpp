#include "discoverdockwidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QMenu>
#include <QClipboard>
#include "logmanager.h"

DiscoverDockWidget::DiscoverDockWidget(QWidget *parent, plcManager *plc)
    : QDockWidget("Discover", parent), lbplc(plc)
{
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout *vbox = new QVBoxLayout(content);
    btnDiscover = new QPushButton("Send Discover");
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->setSpacing(0);
    hbox->addWidget(btnDiscover);
    hbox->addStretch();
    vbox->addLayout(hbox);

    // Создаем таблицу
    table = new QTableWidget(this);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({"Name", "Type", "IPv4", "MAC", "Delays (ms)", "IF"});
    // Выделять строку целиком при клике на любую ячейку
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // разрешить выбирать только одну строку за раз
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    // запретить редактирование ячеек, чтобы они не открывались по двойному клику
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setColumnHidden(6, true);
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
    vbox->addWidget(table);

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QTableView::customContextMenuRequested, this, &DiscoverDockWidget::showContextMenu);


    connect(
        btnDiscover,
        &QPushButton::clicked,
        this,
        &DiscoverDockWidget::startDiscover);

    connect(lbplc, &plcManager::discoverCompleted,
            this, &DiscoverDockWidget::discoverReceived);

    connect(
        table,
        &QTableWidget::cellDoubleClicked,
        this,
        &DiscoverDockWidget::onTableDoubleClicked);
}

void DiscoverDockWidget::startDiscover()
{
    lbplc->startDiscover();
    table->setRowCount(0); // Очищаем старые строки
}

void DiscoverDockWidget::onTableDoubleClicked(int row, int column)
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
    emit deviceSelected(ipv6,name);
}

void DiscoverDockWidget::showContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = table->itemAt(pos);
    if (!item) return;

    QString ipv6 = table->item(item->row(), 6)->text();
    QString name = table->item(item->row(), 0)->text();

    QMenu menu(this);
    QAction *AddDivice = menu.addAction("Добавить");
    // 2. Делаем его жирным
    QFont font = AddDivice->font();
    font.setBold(true);
    AddDivice->setFont(font);
    QAction *getConf = menu.addAction("Запросить конфигурацию");
    QAction *newConf = menu.addAction("Создать новую конфигурацию");
    QAction *copy = menu.addAction("Копировать");
    QAction *Allcopy = menu.addAction("Копировать всё");

    QAction *selectedItem = menu.exec(table->viewport()->mapToGlobal(pos));
    QClipboard *clipboard = QGuiApplication::clipboard();

    if (selectedItem == AddDivice){
        emit deviceSelected(ipv6,name);
    }else if (selectedItem == copy) {
        clipboard->setText(ldmap.value(ipv6).toString());
    }else if (selectedItem == Allcopy) {
        QStringList qstr;
        for (auto i : ldmap) {
            qstr << i.toString();
        }
        clipboard->setText(qstr.join("\n"));
    }else if (selectedItem == getConf) {
        emit deviceSelected(ipv6,name);
        emit requestConfig(ipv6, name);
    }else if (selectedItem == newConf){
        emit newConfig(ipv6, name);
    }
}

// void DiscoverDockWidget::fillTable()
// {

// }

void DiscoverDockWidget::discoverReceived(const QMap<QString, discover::lbinfo> &DiscoverMap)
{
    ldmap = DiscoverMap;
    table->setSortingEnabled(false); // Отключаем сортировку на время вставки для скорости
    int row = 0;
    for (auto it = ldmap.begin(); it != ldmap.end(); ++it) {
        debugPLC() << it.value();
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

        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem *item = table->item(row, col);
            if (item) {
                item->setToolTip(it.key());
                if (it.value().btn) {
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
