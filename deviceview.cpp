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

    disconnect(deviceModel, &QStandardItemModel::dataChanged,
               this, &deviceView::onDataChanged);

    parseSchemaNode(deviceModel->invisibleRootItem(), m_schemaRoot, QJsonValue(yamlRoot));
    // deviceTreeView->expandAll();
    deviceTreeView->setColumnWidth(0, 200);

    modified = false;
    connect(deviceModel, &QStandardItemModel::dataChanged,
               this, &deviceView::onDataChanged);

}

QJsonObject deviceView::getUpdateData()
{
    if (!deviceModel) return QJsonObject();

    // Запускаем сборку от невидимого корня модели
    return serializeNode(deviceModel->invisibleRootItem());
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
        if (key != "any" && !meta.contains("any") && type != "link" && type != "keynum") {
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
            // А) Собираем все ключи из YAML, содержащие подстроку "slot"
            QStringList matchedSlots;
            for (auto yIt = yamlObj.begin(); yIt != yamlObj.end(); ++yIt) {
                if (yIt.key().contains("slot")) {
                    matchedSlots.append(yIt.key());
                }
            }

            // Б) Сортируем слоты по возрастанию номеров (с учетом возможных "slot-1", "slot1", "slot12")
            std::sort(matchedSlots.begin(), matchedSlots.end(), [key](const QString &a, const QString &b) {
                int indexA = a.mid(key.length()).toInt();
                int indexB = b.mid(key.length()).toInt();
                return indexA < indexB;
            });

            // В) Итерируемся по уже ОТСОРТИРОВАННЫМ слотам и строим дерево
            for (const QString &slotKey : matchedSlots) {
                QJsonObject slotData = yamlObj.value(slotKey).toObject();
                QString moduleType = slotData.value("module").toString();

                QJsonObject subSchema = findModuleSchema(moduleType);
                QString moduleName = subSchema.value("name").toString();
                QString moduleDesc = subSchema.value("description").toString();

                QStandardItem *slotNameItem = new QStandardItem(QString("%1 (%2)").arg(slotKey, moduleName));
                slotNameItem->setEditable(false);

                // СОХРАНЯЕМ ДАННЫЕ ДЛЯ ПОСЛЕДУЮЩЕЙ СБОРКИ, ЧТОБЫ НЕ УСЛОЖНЯТЬ ЖИЗНЬ:
                slotNameItem->setData(slotKey, deviceView::SlotKeyRole);
                slotNameItem->setData(moduleType, deviceView::ModuleTypeRole);

                QStandardItem *slotValueItem = new QStandardItem("");
                slotValueItem->setEditable(false);

                QStandardItem *slotDescItem = new QStandardItem(moduleDesc);
                slotDescItem->setEditable(false);

                parentNode->appendRow({slotNameItem, slotValueItem, slotDescItem});

                // Рекурсивно разворачиваем внутреннюю структуру параметров отсортированного модуля
                parseSchemaNode(slotNameItem, subSchema.value("structure").toObject(), QJsonValue(slotData));
            }
            continue;
        }


        // 3. Обработка внутренних линков-диапазонов ("link" или "keynum")
        if ((type == "link" || type == "keynum") && meta.contains("structure")) {
            QJsonObject subStruct = meta.value("structure").toObject();

            // А) Собираем все ключи из YAML, которые начинаются с нужного префикса (например, "holding", "in")
            QStringList matchedYamlKeys;
            for (auto yIt = yamlObj.begin(); yIt != yamlObj.end(); ++yIt) {
                if (yIt.key().startsWith(key)) {
                    matchedYamlKeys.append(yIt.key());
                }
            }

            // Б) Сортируем собранные ключи по возрастанию чисел внутри них (Естественная сортировка)
            std::sort(matchedYamlKeys.begin(), matchedYamlKeys.end(), [key](const QString &a, const QString &b) {
                // Отрезаем префикс (получаем "10" или "0..1")
                QString numStrA = a.mid(key.length());
                QString numStrB = b.mid(key.length());

                // Избавляемся от содержимого диапазонов (если есть "..", отсекаем всё после них)
                int dotIdxA = numStrA.indexOf("..");
                if (dotIdxA != -1) numStrA = numStrA.left(dotIdxA);

                int dotIdxB = numStrB.indexOf("..");
                if (dotIdxB != -1) numStrB = numStrB.left(dotIdxB);

                // Теперь безопасно переводим в int и сравниваем
                return numStrA.toInt() < numStrB.toInt();
            });

            // В) Итерируемся по уже ОТСОРТИРОВАННОМУ списку ключей и строим дерево
            for (const QString &yamlKey : matchedYamlKeys) {
                QJsonValue yamlValue = yamlObj.value(yamlKey);

                QString paramName = yamlKey;
                QString paramValue = "";

                // Если тип keynum — выносим число в колонку Value
                if (type == "keynum") {
                    paramName = key;
                    paramValue = yamlKey.mid(key.length());
                }

                QStandardItem *linkNameItem = new QStandardItem(paramName);
                linkNameItem->setEditable(false);

                QStandardItem *linkValueItem = new QStandardItem(paramValue);
                linkValueItem->setEditable(type == "keynum");
                // Сохраняем оригинальный ключ во вторую колонку (linkValueItem)
                linkValueItem->setData(yamlKey, deviceView::OriginalKeyRole);

                QStandardItem *linkDescItem = new QStandardItem(desc);
                linkDescItem->setEditable(false);

                parentNode->appendRow({linkNameItem, linkValueItem, linkDescItem});

                // Рекурсивный спуск во внутренние параметры
                if (yamlValue.isObject()) {
                    parseSchemaNode(linkNameItem, subStruct, yamlValue);
                } else {
                    if (type == "keynum") {
                        QJsonDocument inlineDoc = QJsonDocument::fromJson(yamlValue.toString().toUtf8());
                        if (inlineDoc.isObject()) {
                            parseSchemaNode(linkNameItem, subStruct, inlineDoc.object());
                        }
                    } else {
                        linkValueItem->setText(yamlValue.toString());
                        linkValueItem->setEditable(true);
                    }
                }
            }
            continue; // Переходим к следующему элементу схемы
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
            paramValueItem->setData(meta, deviceView::SchemaMetaRole);
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

        // Добираемся до значения "value" внутри структуры модуля
        QJsonValue valueNode = mod.value("structure").toObject()
                                   .value("module").toObject()
                                   .value("value");


        // Сценарий 1: "value" — это массив псевдонимов, например ["bcbase", "LB241CPU"]
        if (valueNode.isArray()) {
            QJsonArray aliases = valueNode.toArray();
            for (const QJsonValue &alias : aliases) {
                if (alias.toString() == moduleValue) {
                    return mod; // Нашли совпадение среди псевдонимов!
                }
            }
        }
        // Сценарий 2: "value" — это обычная одиночная строка, например "bcdi"
        else if (valueNode.isString()) {
            if (valueNode.toString() == moduleValue) {
                return mod; // Нашли прямое совпадение
            }
        }
    }
    return QJsonObject();
}

QJsonObject deviceView::serializeNode(QStandardItem *parentNode)
{
    QJsonObject resultObj;

    for (int i = 0; i < parentNode->rowCount(); ++i) {
        QStandardItem *nameItem  = parentNode->child(i, 0);
        QStandardItem *valueItem = parentNode->child(i, 1);

        if (!nameItem) continue;

        QString key = nameItem->text();

        // Читаем метаданные схемы
        QJsonObject meta = valueItem ? valueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();
        QString type = meta.value("type").toString();

        // 1. ПОЛИМОРФНЫЕ СЛОТЫ (Быстрое восстановление "module" и "slot" из UserRole)
        QString savedSlotKey   = nameItem->data(deviceView::SlotKeyRole).toString();
        QString savedModuleType = nameItem->data(deviceView::ModuleTypeRole).toString();

        if (!savedSlotKey.isEmpty()) {
            QJsonObject slotContent = serializeNode(nameItem);
            slotContent.insert("module", savedModuleType);
            resultObj.insert(savedSlotKey, slotContent);
            continue;
        }

        // 2. ЖЕСТКАЯ КОРРЕКТИРОВКА ДЛЯ МАССИВОВ FORTE (Превращаем их строго в QJsonArray)
        QStandardItem *parentItem = parentNode;
        if ((key == "var" || key == "var_out" || type == "sequence") && parentItem && parentItem->text() == "forte") {
            QJsonArray seqArray;
            for (int j = 0; j < nameItem->rowCount(); ++j) {
                QStandardItem *arrayItem = nameItem->child(j, 0);
                if (arrayItem && !arrayItem->text().isEmpty()) {
                    seqArray.append(arrayItem->text());
                }
            }
            resultObj.insert(key, seqArray);
            continue;
        }

        // 3. ОБРАБОТКА ТИПА "keynum" (Превращаем "holding" + "0..1" обратно в "holding0..1")
        if (valueItem && !valueItem->data(deviceView::OriginalKeyRole).toString().isEmpty()) {
            if (nameItem->hasChildren()) {
                QString actualIndex = valueItem->text();
                QString combinedKey = key + actualIndex;

                QJsonObject subContent = serializeNode(nameItem);
                resultObj.insert(combinedKey, subContent);
                continue;
            }
        }

        // 4. СТРУКТУРНЫЕ УЗЛЫ / ПОДРАЗДЕЛЫ (clock, var, modbus_server, rs485)
        if (nameItem->hasChildren()) {
            QJsonObject subObj = serializeNode(nameItem);
            resultObj.insert(key, subObj);
            continue;
        }

        // 5. ОБЫЧНЫЕ ОДИНОЧНЫЕ ПАРАМЕТРЫ (leaf)
        if (valueItem) {
            QString valStr = valueItem->text();

            // Если параметр пустой (например, очищенный natural), не пишем его в JSON вовсе,
            // чтобы парсер lbyaml не генерировал пустые строки в YAML.
            if (valStr.isEmpty()) {
                continue;
            }

            // Защита для булевых значений (если lbyaml принимает true/false без кавычек)
            if (valStr.toLower() == "true" || valStr.toLower() == "false") {
                resultObj.insert(key, valStr.toLower() == "true");
                continue;
            }

            // Проверяем тип, который от нас ждет схема
            if (type == "int") {
                bool isInt = false;
                int intVal = valStr.toInt(&isInt);
                if (isInt) {
                    resultObj.insert(key, intVal);
                    continue;
                }
            }
            else if (type == "double") {
                bool isDouble = false;
                // Заменяем запятую на точку на случай локали
                double dblVal = valStr.replace(",", ".").toDouble(&isDouble);
                if (isDouble) {
                    resultObj.insert(key, dblVal);
                    continue;
                }
            }
            else if (type == "bool" || valStr.toLower() == "true" || valStr.toLower() == "false") {
                resultObj.insert(key, valStr.toLower() == "true");
                continue;
            }

            // ПО УМОЛЧАНИЮ (для enum, string и т.д.): сохраняем строго как СТРОКУ,
            // чтобы не ломать конвертер lbyaml
            resultObj.insert(key, valStr);
        }
    }

    return resultObj;
}


bool deviceView::isModified() const
{
    return modified;
}

void deviceView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    modified = true;
    emit onChanged();
}
