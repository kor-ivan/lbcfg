#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>

deviceView::deviceView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *deviceLayout = new QVBoxLayout(this);
    deviceLayout->setContentsMargins(0, 0, 0, 0);
    deviceTableView = new QTableView(this);
    deviceLayout->addWidget(deviceTableView);

    deviceTableView->setShowGrid(true); // Включаем сетку
    deviceTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять строку целиком
    deviceTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Выделять только одну строку за раз
    deviceTableView->setAlternatingRowColors(true); // Чередование цветов строк для читаемости

    // Настройка шрифта (опционально, можно сделать моноширинным, как редактор)
    // varTableView->setFont(QFont("Courier New", 10));

    // 3. Настраиваем поведение заголовков
    QHeaderView *horHeader = deviceTableView->horizontalHeader();
    horHeader->setSectionResizeMode(QHeaderView::Stretch); // Колонки растягиваются на всю доступную ширину
    horHeader->setVisible(true); // Показываем верхние заголовки

    QHeaderView *vertHeader = deviceTableView->verticalHeader();
    vertHeader->setDefaultSectionSize(22); // Компактная высота строк
    vertHeader->setVisible(false); // Прячем левую нумерацию строк (1, 2, 3...), если она не нужна


    deviceModel = new QStandardItemModel(this);
    deviceTableView->setModel(deviceModel);
    QStringList headers = {"Module", "Parametr", "Value", "Type", "Comment"};
    deviceModel->setHorizontalHeaderLabels(headers);
    deviceLayout->addWidget(deviceTableView);
}
