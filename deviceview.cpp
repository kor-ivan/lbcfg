#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>

deviceView::deviceView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *deviceLayout = new QVBoxLayout(this);
    deviceLayout->setContentsMargins(0, 0, 0, 0);

    deviceTreeView = new QTreeView(this);
    deviceTreeView->setAlternatingRowColors(true);
    deviceTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);

    deviceModel = new QStandardItemModel(this);
    QStringList headers = {"Parameter", "Value", "Description"};
    deviceModel->setHorizontalHeaderLabels(headers);
    deviceTreeView->setModel(deviceModel);

    deviceTreeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    deviceTreeView->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    deviceTreeView->header()->setStretchLastSection(true); // Описание растягивается

    deviceLayout->addWidget(deviceTreeView);

    qDebug()<<"ModulesSchema loaded:"<<loadModulesSchema(":/config/resources/modules_schema.json");


    // deviceTableView = new QTableView(this);
    // deviceLayout->addWidget(deviceTableView);

    // deviceTableView->setShowGrid(true); // Включаем сетку
    // deviceTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять строку целиком
    // deviceTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Выделять только одну строку за раз
    // deviceTableView->setAlternatingRowColors(true); // Чередование цветов строк для читаемости

    // // Настройка шрифта (опционально, можно сделать моноширинным, как редактор)
    // // varTableView->setFont(QFont("Courier New", 10));

    // // 3. Настраиваем поведение заголовков
    // QHeaderView *horHeader = deviceTableView->horizontalHeader();
    // horHeader->setSectionResizeMode(QHeaderView::Stretch); // Колонки растягиваются на всю доступную ширину
    // horHeader->setVisible(true); // Показываем верхние заголовки

    // QHeaderView *vertHeader = deviceTableView->verticalHeader();
    // vertHeader->setDefaultSectionSize(22); // Компактная высота строк
    // vertHeader->setVisible(false); // Прячем левую нумерацию строк (1, 2, 3...), если она не нужна


    // deviceModel = new QStandardItemModel(this);
    // deviceTableView->setModel(deviceModel);
    // QStringList headers = {"Module", "Parametr", "Value", "Type", "Comment"};
    // deviceModel->setHorizontalHeaderLabels(headers);
    // deviceLayout->addWidget(deviceTableView);
}

bool deviceView::loadModulesSchema(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    m_schemaObject = doc.object().value("modules").toObject();
    return true;
}
