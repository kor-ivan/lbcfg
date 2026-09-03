#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMenu>
#include "logmanager.h"

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
    deviceTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(deviceTreeView, &QTreeView::customContextMenuRequested,
            this, &deviceView::showContextMenu);

    deviceLayout->addWidget(deviceTreeView);

    debugApp()<<"ModulesSchema loaded:"<<loadModulesSchema(":/config/resources/modules_schema.json");
}

bool deviceView::loadModulesSchema(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    // qDebug()<< "modules_schema.json opened";

    QJsonParseError error;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    // qDebug()<<error.error<<error.errorString()<<error.offset;
    if (!doc.isObject()) return false;
    // qDebug()<< "QJsonDocument isObject";

    m_schemaRoot = doc.object();
    m_schemaLoaded = !m_schemaRoot.isEmpty();
    return m_schemaLoaded;
}

void deviceView::updateData(lbyaml *parser)
{
    if (!parser || !m_schemaLoaded) return;
    deviceModel->removeRows(0, deviceModel->rowCount());

    QJsonObject yamlRoot = parser->getlbJsonObject();
    // qDebug()<<yamlRoot;

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

    // QJsonObject serializeJson = serializeNode(deviceModel->invisibleRootItem());

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
            // QString varDescription = meta.value("description").toString();
            // А) Создаем сначала КОРНЕВОЙ УЗЕЛ для самой секции (например, "var")
            QStandardItem *sectionNameItem = new QStandardItem(key);
            sectionNameItem->setEditable(false);

            QStandardItem *sectionValueItem = new QStandardItem("");
            sectionValueItem->setEditable(false);
            sectionValueItem->setData(meta, deviceView::SchemaMetaRole);

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

                QStandardItem *varDescItem = new QStandardItem("");
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
            seqValueItem->setData(meta, deviceView::SchemaMetaRole);

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
        // А) Пробуем забрать значение из реального YAML
        if (yamlObj.contains(key)) {
            QJsonValue val = yamlObj.value(key);
            if (val.isDouble()) {
                displayValue = QString::number(val.toDouble());
            } else if (val.isBool()) {
                displayValue = val.toBool() ? "true" : "false";
            } else {
                displayValue = val.toString();
            }
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
        if (type == "sequence") {
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
            resultObj.insert(key, valStr);
        }
    }

    return resultObj;
}


bool deviceView::isModified() const
{
    return modified;
}

void deviceView::resetModified()
{
    // qDebug()<< "deviceView::resetModified modified = false";
    modified = false;
}

void deviceView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    modified = true;
    emit onChanged();
}

void deviceView::showContextMenu(const QPoint &pos)
{
    QModelIndex index = deviceTreeView->indexAt(pos);
    if (!index.isValid()) return;

    QModelIndex nameIndex = index.siblingAtColumn(0);
    QModelIndex valueIndex = index.siblingAtColumn(1);

    QStandardItem *nameItem = deviceModel->itemFromIndex(nameIndex);
    QStandardItem *valueItem = deviceModel->itemFromIndex(valueIndex);
    if (!nameItem) return;

    QJsonObject meta = valueItem ? valueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();
    // qDebug()<<meta;
    QString type = meta.value("type").toString();

    QStandardItem *targetSectionItem = nullptr; // Папка-родитель (куда добавляем)
    QStandardItem *currentItem = nullptr;       // Элемент, по которому кликнули (если внутри папки)
    QJsonObject anyStruct;
    QString varDescription;

    // Режимы контекста
    bool isSequenceMode = (type == "sequence");
    bool isAnyMode = meta.contains("any");
    bool isChildItem = false; // Кликнули по элементу внутри папки


    // Сценарий А: Кликнули на саму секцию "var"
    if (isAnyMode || isSequenceMode) {
        targetSectionItem = nameItem;
        if (isAnyMode)
        {
            QJsonObject anyObj = meta.value("any").toObject();
            anyStruct = anyObj.value("structure").toObject();
            varDescription = anyObj.value("description").toString();
        }
    }
    // Сценарий Б: Кликнули на дочерний элемент секции "var" (на имя переменной вроде dw0)
    else if (nameItem->parent()) {
        QStandardItem *parentNameItem = nameItem->parent();
        QStandardItem *parentValueItem = nullptr;
        if (parentNameItem->parent()) {
            parentValueItem = parentNameItem->parent()->child(parentNameItem->row(), 1);
        } else {
            parentValueItem = deviceModel->item(parentNameItem->row(), 1); // Если родитель в корне
        }
        QJsonObject parentMeta = parentValueItem ? parentValueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();
        targetSectionItem = parentNameItem;
        currentItem = nameItem;
        isChildItem = true;
        if (parentMeta.contains("any")) {
            isAnyMode = true;
            QJsonObject anyObj = parentMeta.value("any").toObject();
            anyStruct = anyObj.value("structure").toObject();
            varDescription = anyObj.value("description").toString();
        } else if (parentMeta.value("type").toString() == "sequence") {
            isSequenceMode = true;
        }
    }
    // qDebug() << isSequenceMode << isAnyMode << type << isChildItem << anyStruct;
    // if (currentItem)
    //     qDebug()<<currentItem->text();
    // else
    //     qDebug()<<&currentItem;
    if (!targetSectionItem) {
        return;
    }

    QMenu menu(this);


    // ЛОГИКА 1: МЕНЮ ДЛЯ МАССИВОВ ТИПА SEQUENCE (forte: var / var_out)
    if (isSequenceMode) {
        if (!isChildItem) {
            menu.addAction("Добавить элемент списка", this, [this, targetSectionItem]() {
                insertAndEditNewRow(targetSectionItem);
            });
        } else if (currentItem) {
            menu.addAction("Удалить элемент списка", this, [this, targetSectionItem, currentItem]() {
                targetSectionItem->removeRow(currentItem->row());
                modified = true;
                emit onChanged();
            });
        }
    }
    // ЛОГИКА 2: ЛОГИКА ДЛЯ СЕКЦИИ С СТРУКТУРОЙ "ANY" (глобальный var)
    else if (isAnyMode && !anyStruct.isEmpty()) {
        if (!isChildItem) {
            menu.addAction(tr("Добавить переменную"), this, [this, targetSectionItem, anyStruct, varDescription]() {
                insertAndEditNewRow(targetSectionItem, varDescription, [this, anyStruct](QStandardItem* insertedNode) {
                    QJsonObject defaultYamlData = createDefaultData(anyStruct);
                    parseSchemaNode(insertedNode, anyStruct, QJsonValue(defaultYamlData));
                });
            });
        } else if (currentItem) {
            menu.addAction(tr("Удалить переменную"), this, [this, targetSectionItem, currentItem]() {
                targetSectionItem->removeRow(currentItem->row());
                modified = true;
                emit onChanged();
            });
            // ДИНАМИЧЕСКИЙ ЭКСПОРТ ПЕРЕМЕННОЙ В FORTE / VAR / VAR_OUT
            QStandardItem *forteSection = nullptr;
            for (int j = 0; j < deviceModel->rowCount(); ++j) {
                if (deviceModel->item(j, 0) && deviceModel->item(j, 0)->text() == "forte") {
                    forteSection = deviceModel->item(j, 0);
                    break;
                }
            }
            QString currentVarName = currentItem->text();
            if (forteSection) {
                // Лямбда-помощник для проверки наличия и добавления переменной в массив forte
                auto tryAddVarToForteArray = [this, currentVarName](QStandardItem* forteSubSection, const QString& menuText) -> QAction* {
                    if (!forteSubSection) return nullptr;

                    // Проверяем, нет ли уже этой переменной в данном списке forte
                    bool alreadyExists = false;
                    for (int j = 0; j < forteSubSection->rowCount(); ++j) {
                        if (forteSubSection->child(j, 0) && forteSubSection->child(j, 0)->text() == currentVarName) {
                            alreadyExists = true;
                            break;
                        }
                    }

                    // Если переменной там нет, возвращаем действие для её добавления
                    if (!alreadyExists) {
                        return new QAction(menuText, this);
                    }
                    return nullptr;
                };
                // Ищем внутренние ветки "var" и "var_out" внутри секции forte
                QStandardItem *forteVarNode = nullptr;
                QStandardItem *forteVarOutNode = nullptr;
                for (int j = 0; j < forteSection->rowCount(); ++j) {
                    if (forteSection->child(j, 0)) {
                        if (forteSection->child(j, 0)->text() == "var") forteVarNode = forteSection->child(j, 0);
                        if (forteSection->child(j, 0)->text() == "var_out") forteVarOutNode = forteSection->child(j, 0);
                    }
                }

                // Создаем действия, если проверки прошли успешно
                QAction *actAddToVar = tryAddVarToForteArray(forteVarNode, "Добавить в forte/var");
                QAction *actAddToVarOut = tryAddVarToForteArray(forteVarOutNode, "Добавить в forte/var_out");

                if (actAddToVar || actAddToVarOut) {
                    if (actAddToVar) {
                        menu.addAction(actAddToVar);
                        connect(actAddToVar, &QAction::triggered, this, [this, forteVarNode, currentVarName]() {
                            QStandardItem *newItem = new QStandardItem(currentVarName);
                            newItem->setEditable(true);
                            forteVarNode->insertRow(0, {newItem, new QStandardItem(), new QStandardItem()});
                            deviceTreeView->expand(forteVarNode->index());
                            modified = true;
                            emit onChanged();
                        });
                    }
                    if (actAddToVarOut) {
                        menu.addAction(actAddToVarOut);
                        connect(actAddToVarOut, &QAction::triggered, this, [this, forteVarOutNode, currentVarName]() {
                            QStandardItem *newItem = new QStandardItem(currentVarName);
                            newItem->setEditable(true);
                            forteVarOutNode->insertRow(0, {newItem, new QStandardItem(), new QStandardItem()});
                            deviceTreeView->expand(forteVarOutNode->index());
                            modified = true;
                            emit onChanged();
                        });
                    }
                }
            }

            // Ищем, какие параметры отсутствуют под текущей переменной
            QStringList existingParams;
            for (int j = 0; j < currentItem->rowCount(); ++j) {
                if (currentItem->child(j, 0)) {
                    existingParams.append(currentItem->child(j, 0)->text());
                }
            }

            QStringList missingParams;
            for (auto it = anyStruct.begin(); it != anyStruct.end(); ++it) {
                if (!existingParams.contains(it.key())) {
                    missingParams.append(it.key());
                }
            }

            if (!missingParams.isEmpty()) {
                QMenu *subMenu = menu.addMenu(tr("Добавить..."));
                for (const QString &missingKey : missingParams) {
                    QJsonObject paramMeta = anyStruct.value(missingKey).toObject();
                    subMenu->addAction(missingKey, this, [this, currentItem, missingKey, paramMeta]() {
                        QJsonObject singleSchema;
                        singleSchema.insert(missingKey, paramMeta);
                        QJsonObject singleData = createDefaultData(singleSchema);

                        parseSchemaNode(currentItem, singleSchema, QJsonValue(singleData));
                        deviceTreeView->expand(currentItem->index());

                        modified = true;
                        emit onChanged();
                    });
                }
            }
        }
    }

    // Отображаем меню на экране
    menu.exec(deviceTreeView->viewport()->mapToGlobal(pos));

}

QJsonObject deviceView::createDefaultData(const QJsonObject &structureSchema)
{
    QJsonObject defaultObj;
    // if (structureSchema.contains("type")) {
    //     // todo later
    // }
    for (auto it = structureSchema.begin(); it != structureSchema.end(); ++it) {
        QJsonObject paramMeta = it.value().toObject();

        if (paramMeta.contains("default")) {
            defaultObj.insert(it.key(), paramMeta.value("default"));
        } else {
            defaultObj.insert(it.key(), ""); // Фоллбэк, если дефолт забыли указать
        }
    }

    return defaultObj;
}
