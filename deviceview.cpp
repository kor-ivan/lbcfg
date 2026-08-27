#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

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
    qDebug()<< "modules_schema.json opened";

    QJsonParseError error;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    qDebug()<<error.error<<error.errorString()<<error.offset;
    if (!doc.isObject()) return false;
    qDebug()<< "QJsonDocument isObject";

    m_schemaRoot = doc.object();
    m_schemaLoaded = !m_schemaRoot.isEmpty();
    return m_schemaLoaded;
}

void deviceView::updateData(lbyaml *parser)
{
    if (!parser || !m_schemaLoaded) return;
    deviceModel->removeRows(0, deviceModel->rowCount());

    QJsonObject yamlRoot = parser->getlbJsonObject();
    qDebug()<<yamlRoot;

    parseSchemaNode(deviceModel->invisibleRootItem(), m_schemaRoot, QJsonValue(yamlRoot));
    // deviceTreeView->expandAll();

}

void deviceView::parseSchemaNode(QStandardItem *parentNode, const QJsonObject &schemaNode, const QJsonValue &yamlData)
{
    QJsonObject yamlObj = yamlData.toObject();

    for (auto it = schemaNode.begin(); it != schemaNode.end(); ++it) {
        QString key = it.key();
        if (key == "module")
            continue;
        QJsonObject meta = it.value().toObject();
        QString desc = meta.value("description").toString();
        QString type = meta.value("type").toString();

        // Исключение: блоки "any" и полиморфные "link", так как они обрабатывают данные динамически
        if (key != "any" && !meta.contains("any") && type != "link") {
            if (!yamlObj.contains(key) || yamlObj.value(key).isUndefined() || yamlObj.value(key).isNull()) {
                continue; // Такого параметра нет в YAML, не создаем для него строку
            }
        }

        // 1. Обработка паттерна "any" (для секции var, где ключи — это имена переменных)
        if (key == "any" || meta.contains("any")) {
            QJsonObject anyMeta = meta.contains("any") ? meta.value("any").toObject() : meta;
            QJsonObject anyStruct = anyMeta.value("structure").toObject();
            // А) Создаем сначала КОРНЕВОЙ УЗЕЛ для самой секции (например, "var")
            QStandardItem *sectionNameItem = new QStandardItem(key);
            sectionNameItem->setEditable(false);

            QStandardItem *sectionValueItem = new QStandardItem("");
            sectionValueItem->setEditable(false);

            QStandardItem *sectionDescItem = new QStandardItem(meta.value("description").toString());
            sectionDescItem->setEditable(false);

            // Добавляем саму секцию в модель
            parentNode->appendRow({sectionNameItem, sectionValueItem, sectionDescItem});

            // Б) Извлекаем объект данных из YAML именно для этой секции
            QJsonObject anyDataObj = yamlObj.value(key).toObject();

            // В) Идем по реальным переменным пользователя и складываем их ВНУТРЬ sectionNameItem
            for (auto yIt = anyDataObj.begin(); yIt != anyDataObj.end(); ++yIt) {
                QStandardItem *varNameItem = new QStandardItem(yIt.key());
                varNameItem->setEditable(false);

                QStandardItem *varValueItem = new QStandardItem("");
                varValueItem->setEditable(false);

                QStandardItem *varDescItem = new QStandardItem("Пользовательская переменная");
                varDescItem->setEditable(false);

                // ВАЖНО: Добавляем строку в созданную секцию, а не в parentNode!
                sectionNameItem->appendRow({varNameItem, varValueItem, varDescItem});

                // Рекурсивно парсим свойства конкретной переменной ({init: 0, retain: y})
                parseSchemaNode(varNameItem, anyStruct, yIt.value());
            }
            continue;
        }
        // 2. Обработка слотов "type": "link" с массивом "modules"
        if (type == "link" && meta.contains("modules")) {
            // Ищем в YAML объекте все ключи, содержащие подстроку "slot" (slot1, slot-1 и т.д.)
            for (auto yIt = yamlObj.begin(); yIt != yamlObj.end(); ++yIt) {
                if (yIt.key().contains("slot")) {
                    QJsonObject slotData = yIt.value().toObject();
                    QString moduleType = slotData.value("module").toString(); // "bcbase", "bcdi", "bcdo"...

                    QJsonObject subSchema = findModuleSchema(moduleType);
                    QString moduleName = subSchema.value("name").toString();
                    QString moduleDesc = subSchema.value("description").toString();

                    QStandardItem *slotNameItem = new QStandardItem(QString("%1 (%2)").arg(yIt.key(), moduleName));
                    slotNameItem->setEditable(false);

                    QStandardItem *slotValueItem = new QStandardItem("");
                    slotValueItem->setEditable(false);

                    QStandardItem *slotDescItem = new QStandardItem(moduleDesc);
                    slotDescItem->setEditable(false);

                    parentNode->appendRow({slotNameItem, slotValueItem, slotDescItem});

                    parseSchemaNode(slotNameItem, subSchema.value("structure").toObject(), QJsonValue(slotData));
                }
            }
            continue;
        }
        // 3. Обработка внутренних вложенных линков-диапазонов (holding, in, out, chan)
        if (type == "link" && meta.contains("structure")){
            QJsonObject subStruct = meta.value("structure").toObject();
            // Ищем в YAML объекте ключи, которые начинаются с имени текущего линка (например, "holding0..15" или "in0")
            for (auto yIt = yamlObj.begin(); yIt != yamlObj.end(); ++yIt){
                if (yIt.key().startsWith(key)){
                    QStandardItem *linkNameItem = new QStandardItem(yIt.key());
                    linkNameItem->setEditable(false);

                    QStandardItem *linkValueItem = new QStandardItem("");
                    linkValueItem->setEditable(false);

                    QStandardItem *linkDescItem = new QStandardItem(desc);
                    linkDescItem->setEditable(false);

                    parentNode->appendRow({linkNameItem, linkValueItem, linkDescItem});

                    // Если значение является вложенным JSON-объектом (развернутая запись параметров)
                    if (yIt.value().isObject()) {
                        parseSchemaNode(linkNameItem, subStruct, yIt.value());
                    } else {
                        // Если запись компактная (inline синтаксис YAML, например holding6: {var_out: ai0})
                        // Выведем строковое представление во вторую колонку
                        linkValueItem->setText(yIt.value().toString());
                        linkValueItem->setEditable(true);
                    }
                }
            }
            continue;
        }
        // 4. Обычные структурные группы параметров (clock, modbus_server, rs485, wdt)
        if (meta.contains("structure") && type != "link") {
            QStandardItem *sectionNameItem = new QStandardItem(key);
            sectionNameItem->setEditable(false);

            QStandardItem *sectionValueItem = new QStandardItem("");
            sectionValueItem->setEditable(false);

            QStandardItem *sectionDescItem = new QStandardItem(meta.value("description").toString());
            sectionDescItem->setEditable(false);

            parentNode->appendRow({sectionNameItem, sectionValueItem, sectionDescItem});

            // Спускаемся глубже по дереву схемы и берем соответствующий вложенный узел из данных ПЛК
            parseSchemaNode(sectionNameItem, meta.value("structure").toObject(), yamlObj.value(key));
            continue;
        }

        // 4.5. Обработка списков / массивов ("type": "sequence" для блоков forte: var/var_out)
        if (type == "sequence") {
            QStandardItem *seqNameItem = new QStandardItem(key);
            seqNameItem->setEditable(false);

            QStandardItem *seqValueItem = new QStandardItem(""); // Сам узел-заголовок значения не имеет
            seqValueItem->setEditable(false);

            QStandardItem *seqDescItem = new QStandardItem(desc);
            seqDescItem->setEditable(false);

            parentNode->appendRow({seqNameItem, seqValueItem, seqDescItem});

            // Извлекаем массив из JSON данных YAML
            QJsonArray jsonArray = yamlObj.value(key).toArray();

            // Заполняем раскрывающийся список элементами из массива YAML
            for (int i = 0; i < jsonArray.size(); ++i) {
                QString itemText = jsonArray.at(i).toString();

                // Если внутри массива лежат не строки, а числа или другие типы
                if (itemText.isEmpty() && jsonArray.at(i).isDouble()) {
                    itemText = QString::number(jsonArray.at(i).toDouble());
                }

                // Создаем строку для элемента списка
                QStandardItem *subItemName = new QStandardItem(itemText);
                subItemName->setEditable(true); // Разрешаем редактировать имя элемента в списке

                QStandardItem *subItemValue = new QStandardItem();
                subItemValue->setEditable(false);

                QStandardItem *subItemDesc = new QStandardItem();
                subItemDesc->setEditable(false);

                // ВАЖНО: Добавляем элемент ВНУТРЬ созданного узла seqNameItem (var или var_out)
                seqNameItem->appendRow({subItemName, subItemValue, subItemDesc});
            }
            continue; // Переходим к следующему элементу схемы, чтобы не свалиться в шаг №5
        }

        // 5. Конечные leaf-параметры (значения свойств: строки, числа, enum, const)
        QStandardItem *paramNameItem = new QStandardItem(key);
        paramNameItem->setEditable(false);
        // Преобразуем JSON-значение в строку для отображения в ячейке TreeView
        QString displayValue;
        QJsonValue val = yamlObj.value(key);
        if (val.isDouble()) {
            displayValue = QString::number(val.toDouble());
        } else if (val.isBool()) {
            displayValue = val.toBool() ? "true" : "false";
        } else {
            displayValue = val.toString();
        }

        QStandardItem *paramValueItem = new QStandardItem(displayValue);
        if (type == "const") {
            paramValueItem->setEditable(false); // Запрещаем редактирование констант (например, module: bcbase)
        } else {
            paramValueItem->setEditable(true);
            // Прикрепляем метаданные текущего узла схемы (будет нужно делегату для отрисовки ComboBox)
            paramValueItem->setData(meta, Qt::UserRole);
        }

        QStandardItem *paramDescItem = new QStandardItem(desc);
        paramDescItem->setEditable(false);

        parentNode->appendRow({paramNameItem, paramValueItem, paramDescItem});
    }
}

QJsonObject deviceView::findModuleSchema(const QString &moduleValue)
{
    // Извлекаем массив "modules" из узла "slot" нашей корневой схемы
    QJsonArray modulesArray = m_schemaRoot.value("slot").toObject().value("modules").toArray();
    for (const QJsonValue &val : modulesArray) {
        QJsonObject mod = val.toObject();
        if (mod.value("structure").toObject().value("module").toObject().value("value").toString() == moduleValue) {
            return mod;
        }
    }
    return QJsonObject();
}
