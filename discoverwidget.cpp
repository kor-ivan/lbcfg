#include "discoverwidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>

discoverWidget::discoverWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    btnDiscover = new QPushButton("Send Discover");
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addWidget(btnDiscover);
    hbox->addStretch();
    vbox->addLayout(hbox);

    // Создаем таблицу
    table = new QTableWidget();
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

    connect(
        btnDiscover,
        &QPushButton::clicked,
        this,
        &discoverWidget::startDiscover);

    connect(
        table,
        &QTableWidget::cellDoubleClicked,
        this,
        &discoverWidget::onTableDoubleClicked);

    wgtdiscover = new discover(this);

    connect(wgtdiscover, &discover::discoverCompleted, this,
            [this] (const QMap<QString, discover::lbinfo>& DiscoverMap, const discover::discoverError error, const QString errorStr){
                discoverRunning = false;
                if (error != discover::NoError)
                    return;
                fillTable(DiscoverMap);
            }
            );
}

void discoverWidget::startDiscover()
{
    if (discoverRunning)
        return;
    discoverRunning = true;
    table->setRowCount(0); // Очищаем старые строки
    wgtdiscover -> execute();

}

void discoverWidget::onTableDoubleClicked(int row, int column)
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

void discoverWidget::fillTable(const QMap<QString, discover::lbinfo> &discoverMap)
{
    table->setSortingEnabled(false); // Отключаем сортировку на время вставки для скорости
    int row = 0;
    for (auto it = discoverMap.begin(); it != discoverMap.end(); ++it) {
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
