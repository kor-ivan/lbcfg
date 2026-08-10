#include "varview.h"
#include <QVBoxLayout>
#include <QHeaderView>


varView::varView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *varLayout = new QVBoxLayout(this);
    varLayout->setContentsMargins(0, 0, 0, 0);
    // 1. Создаем таблицу
    varTableView = new QTableView(this);

    // 2. Настраиваем внешний вид таблицы (современный стиль)
    varTableView->setShowGrid(true); // Включаем сетку
    varTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять строку целиком
    varTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Выделять только одну строку за раз
    varTableView->setAlternatingRowColors(true); // Чередование цветов строк для читаемости

    // Настройка шрифта (опционально, можно сделать моноширинным, как редактор)
    // varTableView->setFont(QFont("Courier New", 10));

    // 3. Настраиваем поведение заголовков
    QHeaderView *horHeader = varTableView->horizontalHeader();
    horHeader->setSectionResizeMode(QHeaderView::Stretch); // Колонки растягиваются на всю доступную ширину
    horHeader->setVisible(true); // Показываем верхние заголовки

    QHeaderView *vertHeader = varTableView->verticalHeader();
    vertHeader->setDefaultSectionSize(22); // Компактная высота строк
    vertHeader->setVisible(false); // Прячем левую нумерацию строк (1, 2, 3...), если она не нужна

    // 4. Создаем модель данных и привязываем её к таблице
    varModel = new QStandardItemModel(this);
    varTableView->setModel(varModel);

    varLayout->addWidget(varTableView);
}

void varView::updateData(const QString &yamlText)
{
    if (!varModel) return;
    varModel->clear();
    QStringList headers = {"Имя переменной", "Тип данных", "Адрес / Регистр", "Описание"};
    varModel->setHorizontalHeaderLabels(headers);

    // Временная заглушка для тестов, чтобы увидеть, что таблица работает:
    for (int i = 0; i < 5; ++i) {
        varModel->setItem(i, 0, new QStandardItem(QString("var_test_%1").arg(i)));
        varModel->setItem(i, 1, new QStandardItem("INT"));
        varModel->setItem(i, 2, new QStandardItem(QString("MW%1").arg(i * 2)));
        varModel->setItem(i, 3, new QStandardItem("Тестовое описание переменной"));
    }
}
