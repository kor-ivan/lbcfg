#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMenu>

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
                int indexA = QStringView(a).mid(key.length()).toInt();
                int indexB = QStringView(b).mid(key.length()).toInt();
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
                linkValueItem->setData(meta, deviceView::SchemaMetaRole);

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
            sectionValueItem->setData(meta, deviceView::SchemaMetaRole);

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
    qDebug()<< "deviceView::resetModified modified = false";
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

    // 1. ЭТАП АНАЛИЗА: Получаем чистый контекст точки клика
    ContextMenuContext ctx = analyzeMenuContext(index);
    if (!ctx.isValidClick) return;
    QMenu menu(this);

    // БЛОК 1: КЛИК ПО ПУСТОМУ ПРОСТРАНСТВУ (Глобальное конструирование)
    if (ctx.isBlankSpace) {
        // А) Сканируем текущую модель, чтобы узнать, какие блоки уже добавлены в корень
        QStringList existingRootItems;
        for (int i = 0; i < deviceModel->rowCount(); ++i) {
            if (deviceModel->item(i, 0)) {
                QString name = deviceModel->item(i, 0)->text();
                // Для слотов собираем общую маску "slot", так как их может быть много
                existingRootItems.append(name.contains("slot") ? "slot" : name);
            }
        }

        // Б) Находим базовые блоки из схемы, которых ещё нет на экране (forte, clock, ipaddr...)
        QStringList missingRootBlocks;
        bool hasSlotSpecification = m_schemaRoot.contains("slot");

        for (auto it = m_schemaRoot.begin(); it != m_schemaRoot.end(); ++it) {
            if (it.key() != "slot" && !existingRootItems.contains(it.key())) {
                missingRootBlocks.append(it.key());
            }
        }

        // ДЕЙСТВИЕ 1: Выводим меню для обычных пропущенных блоков
        if (!missingRootBlocks.isEmpty()) {
            QMenu *addBlockMenu = menu.addMenu(tr("Добавить блок конфигурации"));
            for (const QString &blockKey : missingRootBlocks) {
                QJsonObject blockMeta = m_schemaRoot.value(blockKey).toObject();
                QString blockDesc = blockMeta.value("description").toString();
                addBlockMenu->addAction(blockKey, this, [this, blockKey, blockMeta, blockDesc]() {
                    QJsonObject defaultData;
                    QJsonObject subSchema = blockMeta.contains("structure") ? blockMeta.value("structure").toObject() : m_schemaRoot;
                    if (blockMeta.contains("structure")) {
                        // Для сложных блоков (clock, forte) генерируем дефолтный объект и пускаем в рекурсию
                        defaultData.insert(blockKey, createDefaultData(subSchema));
                        parseSchemaNode(deviceModel->invisibleRootItem(), m_schemaRoot, QJsonValue(defaultData));
                    }
                    else{
                        // Для плоских параметров в корне (ipaddr, gateway) сразу создаем строку ячеек
                        QStandardItem *pName = new QStandardItem(blockKey);
                        pName->setEditable(false);
                        QStandardItem *pValue = new QStandardItem("");
                        pValue->setEditable(true);
                        pValue->setData(blockMeta, deviceView::SchemaMetaRole);
                        QStandardItem *pDesc = new QStandardItem(blockDesc);
                        pDesc->setEditable(false);
                        deviceModel->invisibleRootItem()->appendRow({pName, pValue, pDesc});
                    }
                    modified = true;
                    emit onChanged();
                });
            }
        }
        // ДЕЙСТВИЕ 2: Выводим подменю для добавления нового СЛОТА железа
        if (hasSlotSpecification) {
            QMenu *addSlotMenu = menu.addMenu(tr("Добавить модуль в новый слот"));
            QJsonObject slotMeta = m_schemaRoot.value("slot").toObject();
            QJsonArray availableModules = slotMeta.value("modules").toArray();

            // АНАЛИЗ ТЕКУЩЕЙ ТОПОЛОГИИ ЧЕРЕЗ USER_ROLE
            int currentBaseCount = 0;
            int currentIoCount = 0;

            // Считаем текущее количество для проверки лимитов
            for (int i = 0; i < deviceModel->rowCount(); ++i) {
                QStandardItem *rootItem = deviceModel->item(i, 0);
                if (!rootItem) continue;
                QString savedSlotKey = rootItem->data(deviceView::SlotKeyRole).toString();
                if (savedSlotKey.startsWith("slot")) {
                    // Оптимизация clazy: используем QStringView вместо mid для проверки типа
                    if (QStringView(savedSlotKey).mid(4).toInt() < 0)
                        currentBaseCount++;
                    else
                        currentIoCount++;
                }
            }
            for (const QJsonValue &modVal : availableModules) {
                QJsonObject modObj = modVal.toObject();
                QString modName = modObj.value("name").toString();
                QString modDesc = modObj.value("description").toString();
                QJsonObject modStructure = modObj.value("structure").toObject();

                bool isBaseModule = modObj.contains("base");
                bool isIoModule = modObj.contains("io");
                int maxAllowed;
                int currentCount;
                if (isBaseModule){
                    maxAllowed = modObj.value("base").toInt();
                    currentCount = currentBaseCount;
                }
                if (isIoModule){
                    maxAllowed = modObj.value("io").toInt();
                    currentCount = currentIoCount;
                }

                QAction *modAction = new QAction(modName, this);
                if (currentCount >= maxAllowed) {
                    modAction->setEnabled(false);
                    modAction->setText(QString("%1 [%2]").arg(modAction->text(), tr("ЛИМИТ ДОСТИГНУТ")));
                }
                addSlotMenu->addAction(modAction);
                connect(modAction, &QAction::triggered, this, [this, modStructure, modName, modDesc, isBaseModule]() {
                    QStandardItem *slotNameItem = new QStandardItem(QString("slot_temp (%1)").arg(modName));
                    slotNameItem->setEditable(false);

                    // Помечаем временный SlotKeyRole, чтобы хелпер reindex понял, базовая это плата или IO
                    slotNameItem->setData(isBaseModule ? "slot-99" : "slot99", deviceView::SlotKeyRole);

                    QString rawModuleType = modStructure.value("module").toObject().value("value").toString();
                    if (rawModuleType.isEmpty() && modStructure.value("module").toObject().value("value").isArray()) {
                        rawModuleType = modStructure.value("module").toObject().value("value").toArray().at(0).toString();
                    }
                    slotNameItem->setData(rawModuleType, deviceView::ModuleTypeRole);

                    QStandardItem *slotValueItem = new QStandardItem("");
                    slotValueItem->setEditable(false);

                    QStandardItem *slotDescItem = new QStandardItem(modDesc);
                    slotDescItem->setEditable(false);
// определение места вставки
                    int insertRowIdx = -1;

                    // Сначала соберем статистику строк: где в модели начинается и заканчивается железо
                    int firstAnySlotRow = -1;
                    int lastAnySlotRow = -1;

                    for (int r = 0; r < deviceModel->rowCount(); ++r) {
                        QStandardItem *item = deviceModel->item(r, 0);
                        if (item) {
                            QString key = item->data(deviceView::SlotKeyRole).toString();
                            if (key.startsWith("slot")) {
                                if (firstAnySlotRow == -1) firstAnySlotRow = r;
                                lastAnySlotRow = r;
                            }
                        }
                    }

                    if (isBaseModule) {
                        // А) БАЗОВЫЙ МОДУЛЬ: должен вставать в самый верх блока слотов
                        if (firstAnySlotRow != -1) {
                            // Если на шасси уже есть хоть какие-то модули (base или io),
                            // встаем на место самого первого из них, сдвигая остальные вниз
                            insertRowIdx = firstAnySlotRow;
                        } else {
                            // Если слотов в модели вообще нет, ищем, перед чем нам приткнуться.
                            // Мы должны встать перед секциями "forte" или "var", но ПОСЛЕ "clock/ipaddr",
                            // чтобы системные настройки оставались в самом верху.
                            insertRowIdx = deviceModel->rowCount();
                            for (int r = 0; r < deviceModel->rowCount(); ++r) {
                                if (deviceModel->item(r, 0)) {
                                    QString txt = deviceModel->item(r, 0)->text();
                                    if (txt == "forte" || txt == "var") {
                                        insertRowIdx = r;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        // Б) IO МОДУЛЬ: должен дописываться в самый конец блока слотов
                        if (lastAnySlotRow != -1) {
                            // Если слоты уже есть, встаем строго ПОСЛЕ самого последнего из них
                            insertRowIdx = lastAnySlotRow + 1;
                        } else {
                            // Если слотов вообще нет, ищем позицию перед forte/var, как и для base
                            insertRowIdx = deviceModel->rowCount();
                            for (int r = 0; r < deviceModel->rowCount(); ++r) {
                                if (deviceModel->item(r, 0)) {
                                    QString txt = deviceModel->item(r, 0)->text();
                                    if (txt == "forte" || txt == "var") {
                                        insertRowIdx = r;
                                        break;
                                    }
                                }
                            }
                        }
                    }
// завершение определения места вставки
                    deviceModel->invisibleRootItem()->insertRow(insertRowIdx, {slotNameItem, slotValueItem, slotDescItem});
                    reindexSlotsOfType(isBaseModule);
                    // Наполняем вложенные параметры
                    QJsonObject defaultSlotData = createDefaultData(modStructure);
                    parseSchemaNode(slotNameItem, modStructure, QJsonValue(defaultSlotData));
                    deviceTreeView->expand(slotNameItem->index());

                    modified = true;
                    emit onChanged();
                });

            }
        }
        if (!menu.isEmpty()) {
            menu.exec(deviceTreeView->viewport()->mapToGlobal(pos));
        }
        return; // Полностью выходим из метода, так как клик по пустоте обработан
    }
    // БЛОК 2: КЛИК ПО СУЩЕСТВУЮЩИМ СТРОКАМ ДЕРЕВА
    else {
        // УНИВЕРСАЛЬНЫЕ ИНТЕРФЕЙСНЫЕ ОПЦИИ ПЕРЕМЕЩЕНИЯ И УДАЛЕНИЯ МОДУЛЕЙ
        QString clickedSlotKey = ctx.targetSectionItem ? ctx.targetSectionItem->data(deviceView::SlotKeyRole).toString() : "";
        bool isRootNode = ctx.targetSectionItem && !ctx.targetSectionItem->parent();

        if (isRootNode) {
            // А) Если это модуль шасси (слот) — выводим опции перемещения вверх/вниз
            if (clickedSlotKey.startsWith("slot")) {
                int slotNum = QStringView(clickedSlotKey).mid(4).toInt();
                bool isBaseType = (slotNum < 0);
                int currentItemRow = ctx.targetSectionItem->row();

                int firstGroupRow = -1, lastGroupRow = -1;
                for (int r = 0; r < deviceModel->rowCount(); ++r) {
                    QStandardItem *item = deviceModel->item(r, 0);
                    if (item && item->data(deviceView::SlotKeyRole).toString().startsWith("slot")) {
                        if ((QStringView(item->data(deviceView::SlotKeyRole).toString()).mid(4).toInt() < 0) == isBaseType) {
                            if (firstGroupRow == -1) firstGroupRow = r;
                            lastGroupRow = r;
                        }
                    }
                }

                QAction *actMoveUp = menu.addAction(tr("Переместить модуль вверх"));
                QAction *actMoveDown = menu.addAction(tr("Переместить модуль вниз"));
                if (currentItemRow == firstGroupRow) actMoveUp->setEnabled(false);
                if (currentItemRow == lastGroupRow) actMoveDown->setEnabled(false);

                connect(actMoveUp, &QAction::triggered, this, [this, currentItemRow, isBaseType]() {
                    deviceModel->invisibleRootItem()->insertRow(currentItemRow - 1, deviceModel->invisibleRootItem()->takeRow(currentItemRow));
                    reindexSlotsOfType(isBaseType);
                    modified = true; emit onChanged();
                });
                connect(actMoveDown, &QAction::triggered, this, [this, currentItemRow, isBaseType]() {
                    deviceModel->invisibleRootItem()->insertRow(currentItemRow + 1, deviceModel->invisibleRootItem()->takeRow(currentItemRow));
                    reindexSlotsOfType(isBaseType);
                    modified = true; emit onChanged();
                });
            }

            // Б) Универсальная кнопка удаления любой корневой секции первого уровня (slot, clock, forte...)
            QString actionText = clickedSlotKey.startsWith("slot") ? tr("Удалить модуль со слота") : tr("Удалить блок конфигурации");
            menu.addAction(actionText, this, [this, ctx, clickedSlotKey]() {
                deviceModel->invisibleRootItem()->removeRow(ctx.targetSectionItem->row());
                if (clickedSlotKey.startsWith("slot")) {
                    reindexSlotsOfType(QStringView(clickedSlotKey).mid(4).toInt() < 0);
                }
                modified = true; emit onChanged();
            });
            menu.addSeparator();
        }

        // ВЕТКА 2.1: Работа с массивами SEQUENCE (forte)
        if (ctx.isSequenceMode) {
            if (!ctx.isChildItem) {
                menu.addAction("Добавить элемент списка", this, [this, ctx]() { insertAndEditNewRow(ctx.targetSectionItem); });
            } else if (ctx.currentItem) {
                menu.addAction("Удалить элемент списка", this, [this, ctx]() {
                    ctx.targetSectionItem->removeRow(ctx.currentItem->row());
                    modified = true;
                    emit onChanged();
                });
            }
        }
        // ВЕТКА 2.2: Работа со сложными структурами (секциями any, каналами и вложенными параметрами плат)
        else {
            // Добавление новой тег-переменной (только для глобальной секции "var" проекта)
            if (ctx.isAnyMode && !ctx.isChildItem) {
                menu.addAction(tr("Добавить переменную"), this, [this, ctx]() {
                    insertAndEditNewRow(ctx.targetSectionItem, ctx.varDescription, [this, ctx](QStandardItem* insertedNode) {
                        parseSchemaNode(insertedNode, ctx.anyStruct, QJsonValue(createDefaultData(ctx.anyStruct)));
                    });
                });
            }

            // Универсальное удаление любого вложенного дочернего параметра или целого канала
            if (ctx.isChildItem && ctx.currentItem && ctx.currentItem->text() != "module") {
                QString delText = ctx.currentItem->hasChildren() ? tr("Удалить канал/группу") : tr("Удалить параметр");
                menu.addAction(delText, this, [this, ctx]() {
                    ctx.targetSectionItem->removeRow(ctx.currentItem->row());
                    modified = true; emit onChanged();
                });
                menu.addSeparator();
            }

            // УМНЫЙ АНАЛИЗ И ВОССТАНОВЛЕНИЕ СТЁРТЫХ ПАРАМЕТРОВ / КАНАЛОВ ИЗ СХЕМЫ
            if (!ctx.anyStruct.isEmpty() && ctx.targetSectionItem) {
                QStandardItem *menuTargetItem = ctx.targetSectionItem;
                // 1. Собираем всё, что УЖЕ присутствует в UI на текущем уровне
                QStringList existingItemsInUi;
                for (int j = 0; j < menuTargetItem->rowCount(); ++j) {
                    if (menuTargetItem->child(j, 0)) {
                        existingItemsInUi.append(menuTargetItem->child(j, 0)->text());
                    }
                }

                // Разворачиваем все существующие маски диапазонов (например, если есть "out0..15", expandVar вернет out0, out1... out15)
                QStringList expandedUiItems;
                for (const QString &uiKey : existingItemsInUi) {
                    expandedUiItems.append(lbyaml::expandVar(uiKey, nullptr));
                }

                QStringList missingFixedParams;
                // Структура для хранения доступных вариантов восстановления динамических каналов
                struct MissingLinkOption {
                    QString menuLabel;      // Что пишем в меню (например, "out0..15" или "out4")
                    QString insertKey;      // Какой ключ запишется в YAML (например, "out0..15" или "out4")
                    QJsonObject linkMeta;   // Метаданные схемы этого линка
                    bool isWholeRange;      // Флаг: это полный диапазон или одиночный канал
                };
                QList<MissingLinkOption> missingLinkOptions;

                // 2. Сканируем схему на предмет пропущенных элементов
                for (auto it = ctx.anyStruct.begin(); it != ctx.anyStruct.end(); ++it) {
                    QString schemaKey = it.key(); // Например, "in", "out", "chan", "init"
                    QJsonObject paramMeta = it.value().toObject();
                    QString pType = paramMeta.value("type").toString();

                    if ((pType == "link" || pType == "keynum") && paramMeta.contains("structure")) {
                        int maxChannels = paramMeta.contains("max") ? paramMeta.value("max").toInt() : 1;
                        bool isRangeStyle = paramMeta.value("range").toBool(false);

                        // Проверяем поштучно каждый виртуальный канал от 0 до max-1
                        QStringList missingIndividualChannels;
                        for (int c = 0; c < maxChannels; ++c) {
                            QString individualKey = QString("%1%2").arg(schemaKey).arg(c); // "out0", "out1"...
                            if (!expandedUiItems.contains(individualKey) && !existingItemsInUi.contains(individualKey)) {
                                missingIndividualChannels.append(individualKey);
                            }
                        }

                        // Если вообще ни одного канала этой группы в UI еще нет,
                        // и схема просит групповой range — предлагаем восстановить ЦЕЛИКОМ ВЕСЬ ДИАПАЗОН
                        if (isRangeStyle && missingIndividualChannels.size() == maxChannels) {
                            // ИСПРАВЛЕНО: безопасная склейка строк без маркера %10
                            QString rangeKey = schemaKey + "0.." + QString::number(maxChannels - 1); // "out0..15"

                            MissingLinkOption opt;
                            opt.menuLabel = rangeKey;
                            opt.insertKey = rangeKey;
                            opt.linkMeta = paramMeta;
                            opt.isWholeRange = true;
                            missingLinkOptions.append(opt);
                        }

                        // Плюс ВСЕГДА (или если диапазон уже разбит/частично удален) предлагаем добавить
                        // недостающие каналы по отдельности, чтобы у инженера был выбор
                        for (const QString &missingChan : missingIndividualChannels) {
                            MissingLinkOption opt;
                            opt.menuLabel = missingChan; // "out0", "out1"...
                            opt.insertKey = missingChan;
                            opt.linkMeta = paramMeta;
                            opt.isWholeRange = false;
                            missingLinkOptions.append(opt);
                        }
                    } else {
                        // Обычный фиксированный параметр схемы (например, init)
                        if (!existingItemsInUi.contains(schemaKey) && schemaKey != "module") {
                            missingFixedParams.append(schemaKey);
                        }
                    }
                }

                // 3. ОТРИСОВКА ВСПЛЫВАЮЩЕГО ПОДМЕНЮ С ВОССТАНОВЛЕНИЕМ
                if (!missingFixedParams.isEmpty() || !missingLinkOptions.isEmpty()) {
                    QMenu *addMissingMenu = menu.addMenu(tr("Добавить..."));

                    // А) Фиксированные параметры модуля (init и др.)
                    for (const QString &missingKey : missingFixedParams) {
                        addMissingMenu->addAction(missingKey, this, [this, menuTargetItem, missingKey, ctx]() {
                            QJsonObject singleSchema;
                            singleSchema.insert(missingKey, ctx.anyStruct.value(missingKey).toObject());
                            parseSchemaNode(menuTargetItem, singleSchema, QJsonValue(createDefaultData(singleSchema)));
                            deviceTreeView->expand(menuTargetItem->index());
                            modified = true; emit onChanged();
                        });
                    }

                    if (!missingFixedParams.isEmpty() && !missingLinkOptions.isEmpty()) {
                        addMissingMenu->addSeparator(); // Разделитель между свойствами и каналами
                    }

                    // Б) Умные динамические каналы (диапазоны или одиночные входы/выходы)
                    for (const MissingLinkOption &opt : missingLinkOptions) {
                        addMissingMenu->addAction(opt.menuLabel, this, [this, menuTargetItem, opt, index, ctx]() {
                            QJsonObject subStruct = opt.linkMeta.value("structure").toObject();
                            QJsonObject defaultSubData = createDefaultData(subStruct);

                            // Вычисляем чистый постфикс канала/диапазона для автоподстановки в имя переменной
                            // Ищем, где кончается буквенный префикс схемы
                            QString rawSchemaKey;
                            for (auto it = ctx.anyStruct.begin(); it != ctx.anyStruct.end(); ++it) {
                                if (it.value().toObject() == opt.linkMeta) { rawSchemaKey = it.key(); break; }
                            }

                            QString postfix = opt.insertKey.mid(rawSchemaKey.length()); // Получим "0..15" или "4"
                            applyVariablePostfix(defaultSubData, subStruct, postfix);

                            // Обертываем в объекты под вычисленным ключом для парсера
                            QJsonObject singleSchema; singleSchema.insert(opt.insertKey, opt.linkMeta);
                            QJsonObject singleData; singleData.insert(opt.insertKey, defaultSubData);

                            parseSchemaNode(menuTargetItem, singleSchema, QJsonValue(singleData));
                            deviceTreeView->expand(menuTargetItem->index());
                            modified = true; emit onChanged();
                        });
                    }
                }
            }
        }
    }
    // // ПЕРЕМЕЩЕНИЕ МОДУЛЕЙ (СЛОТОВ) ВВЕРХ И ВНИЗ ПО ШАССИ
    // QString clickedSlotKey = ctx.targetSectionItem->data(deviceView::SlotKeyRole).toString();

    // // Проверяем, что кликнули по корневому узлу слота (у него нет родителя в модели)
    // if (ctx.targetSectionItem && !ctx.targetSectionItem->parent() && clickedSlotKey.startsWith("slot")) {
    //     int slotNum = QStringView(clickedSlotKey).mid(4).toInt();
    //     bool isBaseType = (slotNum < 0);

    //     int currentItemRow = ctx.targetSectionItem->row();

    //     // Находим границы группы модулей текущего типа в модели
    //     int firstGroupRow = -1;
    //     int lastGroupRow = -1;

    //     for (int r = 0; r < deviceModel->rowCount(); ++r) {
    //         QStandardItem *item = deviceModel->item(r, 0);
    //         if (item) {
    //             QString key = item->data(deviceView::SlotKeyRole).toString();
    //             if (key.startsWith("slot")) {
    //                 bool itemIsBase = (QStringView(key).mid(4).toInt() < 0);
    //                 if (itemIsBase == isBaseType) {
    //                     if (firstGroupRow == -1) firstGroupRow = r;
    //                     lastGroupRow = r;
    //                 }
    //             }
    //         }
    //     }

    //     // Создаем действия для перемещения
    //     QAction *actMoveUp = new QAction(tr("Переместить модуль вверх"), this);
    //     QAction *actMoveDown = new QAction(tr("Переместить модуль вниз"), this);

    //     // Блокируем кнопки, если двигать некуда (крайние элементы группы)
    //     if (currentItemRow == firstGroupRow) actMoveUp->setEnabled(false);
    //     if (currentItemRow == lastGroupRow) actMoveDown->setEnabled(false);

    //     menu.addAction(actMoveUp);
    //     menu.addAction(actMoveDown);

    //     // Логика перемещения ВВЕРХ
    //     connect(actMoveUp, &QAction::triggered, this, [this, currentItemRow, isBaseType]() {
    //         // Извлекаем строку (индекс текущей строки уменьшается на 1)
    //         QList<QStandardItem*> rowItems = deviceModel->invisibleRootItem()->takeRow(currentItemRow);

    //         // Вставляем на одну позицию выше
    //         deviceModel->invisibleRootItem()->insertRow(currentItemRow - 1, rowItems);

    //         // Автоматически обновляем имена слотов и роли под новую топологию
    //         reindexSlotsOfType(isBaseType);

    //         modified = true;
    //         emit onChanged();
    //     });

    //     // Логика перемещения ВНИЗ
    //     connect(actMoveDown, &QAction::triggered, this, [this, currentItemRow, isBaseType]() {
    //         // Извлекаем строку
    //         QList<QStandardItem*> rowItems = deviceModel->invisibleRootItem()->takeRow(currentItemRow);

    //         // Вставляем на одну позицию ниже (так как строка удалилась, старый индекс + 1 — это как раз место под соседом)
    //         deviceModel->invisibleRootItem()->insertRow(currentItemRow + 1, rowItems);

    //         // Автоматически обновляем имена слотов и роли под новую топологию
    //         reindexSlotsOfType(isBaseType);

    //         modified = true;
    //         emit onChanged();
    //     });

    //     menu.addSeparator(); // Черта перед другими действиями
    // }
    // // ВЕТКА 2.1: Работа с массивами / списками (тип "sequence", например, forte)
    // if (ctx.isSequenceMode) {
    //     // Сценарий А: Кликнули на саму папку-заголовок массива (например, "var" или "var_out")
    //     if (!ctx.isChildItem) {
    //         menu.addAction("Добавить элемент списка", this, [this, ctx]() {
    //             // Используем наш универсальный шаблон! Имя сгенерируется само как var_1, var_out_1
    //             insertAndEditNewRow(ctx.targetSectionItem);
    //         });
    //     }
    //     // Сценарий Б: Кликнули на конкретную переменную внутри списка (например, на "do0..15")
    //     else if (ctx.currentItem) {
    //         menu.addAction("Удалить элемент списка", this, [this, ctx]() {
    //             ctx.targetSectionItem->removeRow(ctx.currentItem->row());
    //             modified = true;
    //             emit onChanged();
    //         });
    //     }
    // }
    // // ВЕТКА 2.2: Работа с глобальной секцией переменных проекта (структура "any")
    // else if (ctx.isAnyMode && !ctx.anyStruct.isEmpty()) {
    //     // Сценарий А: Кликнули на заголовок секции "var" в корне проекта
    //     if (!ctx.isChildItem) {
    //         menu.addAction(tr("Добавить переменную"), this, [this, ctx]() {
    //             // Передаем в шаблон описание и лямбду для рекурсивного выращивания параметров (init, retain)
    //             insertAndEditNewRow(ctx.targetSectionItem, ctx.varDescription, [this, ctx](QStandardItem* insertedNode) {
    //                 QJsonObject defaultYamlData = createDefaultData(ctx.anyStruct);
    //                 parseSchemaNode(insertedNode, ctx.anyStruct, QJsonValue(defaultYamlData));
    //             });
    //         });
    //     }
    //     // Сценарий Б: Кликнули на конкретную глобальную переменную (например, "dw0")
    //     else if (ctx.currentItem) {
    //         QString currentVarName = ctx.currentItem->text();

    //         menu.addAction(tr("Удалить переменную"), this, [this, ctx]() {
    //             ctx.targetSectionItem->removeRow(ctx.currentItem->row());
    //             modified = true;
    //             emit onChanged();
    //         });

    //         // --- ДОПОЛНИТЕЛЬНО: Быстрый экспорт переменной в Forte ---
    //         QStandardItem *forteSection = nullptr;
    //         for (int j = 0; j < deviceModel->rowCount(); ++j) {
    //             if (deviceModel->item(j, 0) && deviceModel->item(j, 0)->text() == "forte") {
    //                 forteSection = deviceModel->item(j, 0);
    //                 break;
    //             }
    //         }

    //         if (forteSection) {
    //             // Лямбда-помощник проверяет, нет ли уже тега в forte/var или forte/var_out
    //             auto tryAddVarToForteArray = [this, currentVarName](QStandardItem* forteSubSection, const QString& menuText) -> QAction* {
    //                 if (!forteSubSection) return nullptr;
    //                 bool alreadyExists = false;
    //                 for (int j = 0; j < forteSubSection->rowCount(); ++j) {
    //                     if (forteSubSection->child(j, 0) && forteSubSection->child(j, 0)->text() == currentVarName) {
    //                         alreadyExists = true;
    //                         break;
    //                     }
    //                 }
    //                 if (!alreadyExists) return new QAction(menuText, this);
    //                 return nullptr;
    //             };

    //             QStandardItem *forteVarNode = nullptr;
    //             QStandardItem *forteVarOutNode = nullptr;
    //             for (int j = 0; j < forteSection->rowCount(); ++j) {
    //                 if (forteSection->child(j, 0)) {
    //                     if (forteSection->child(j, 0)->text() == "var") forteVarNode = forteSection->child(j, 0);
    //                     if (forteSection->child(j, 0)->text() == "var_out") forteVarOutNode = forteSection->child(j, 0);
    //                 }
    //             }

    //             QAction *actAddToVar = tryAddVarToForteArray(forteVarNode, "Добавить в forte/var");
    //             QAction *actAddToVarOut = tryAddVarToForteArray(forteVarOutNode, "Добавить в forte/var_out");

    //             if (actAddToVar) {
    //                 menu.addAction(actAddToVar);
    //                 connect(actAddToVar, &QAction::triggered, this, [this, forteVarNode, currentVarName]() {
    //                     QStandardItem *newItem = new QStandardItem(currentVarName);
    //                     newItem->setEditable(true);
    //                     forteVarNode->insertRow(0, {newItem, new QStandardItem(), new QStandardItem()});
    //                     deviceTreeView->expand(forteVarNode->index());
    //                     modified = true;
    //                     emit onChanged();
    //                 });
    //             }
    //             if (actAddToVarOut) {
    //                 menu.addAction(actAddToVarOut);
    //                 connect(actAddToVarOut, &QAction::triggered, this, [this, forteVarOutNode, currentVarName]() {
    //                     QStandardItem *newItem = new QStandardItem(currentVarName);
    //                     newItem->setEditable(true);
    //                     forteVarOutNode->insertRow(0, {newItem, new QStandardItem(), new QStandardItem()});
    //                     deviceTreeView->expand(forteVarOutNode->index());
    //                     modified = true;
    //                     emit onChanged();
    //                 });
    //             }
    //         }

    //         // --- ДОПОЛНИТЕЛЬНО: Добавление стёртых / отсутствующих полей (init, retain) ---
    //         QStringList existingParams;
    //         for (int j = 0; j < ctx.currentItem->rowCount(); ++j) {
    //             if (ctx.currentItem->child(j, 0)) {
    //                 existingParams.append(ctx.currentItem->child(j, 0)->text());
    //             }
    //         }

    //         QStringList missingParams;
    //         for (auto it = ctx.anyStruct.begin(); it != ctx.anyStruct.end(); ++it) {
    //             if (!existingParams.contains(it.key())) {
    //                 missingParams.append(it.key());
    //             }
    //         }

    //         if (!missingParams.isEmpty()) {
    //             QMenu *subMenu = menu.addMenu(tr("Добавить..."));
    //             for (const QString &missingKey : missingParams) {
    //                 QJsonObject paramMeta = ctx.anyStruct.value(missingKey).toObject();
    //                 subMenu->addAction(missingKey, this, [this, ctx, missingKey, paramMeta]() {
    //                     QJsonObject singleSchema;
    //                     singleSchema.insert(missingKey, paramMeta);
    //                     QJsonObject singleData = createDefaultData(singleSchema);

    //                     parseSchemaNode(ctx.currentItem, singleSchema, QJsonValue(singleData));
    //                     deviceTreeView->expand(ctx.currentItem->index());

    //                     modified = true;
    //                     emit onChanged();
    //                 });
    //             }
    //         }
    //     }
    // }

    // БЛОК 3: ОБЩИЕ ПУНКТЫ МЕНЯ ДЛЯ ВСЕХ СЛУЧАЕВ КЛИКАПО СТРОКАМ
    menu.addSeparator();
    menu.addAction(tr("Развернуть всё дерево"), deviceTreeView, &QTreeView::expandAll);
    menu.addAction(tr("Свернуть всё дерево"), deviceTreeView, &QTreeView::collapseAll);
    // // БЛОК N: Для удаления элемента из дерева
    // if (ctx.targetSectionItem && !ctx.targetSectionItem->parent()) {
    //     menu.addSeparator();
    //     QString clickedSlotKey = ctx.targetSectionItem->data(deviceView::SlotKeyRole).toString();
    //     bool isSlot = clickedSlotKey.startsWith("slot");
    //     QString actionText = isSlot ? tr("Удалить модуль со слота") : tr("Удалить блок конфигурации");

    //     menu.addAction(actionText, this, [this, ctx, isSlot, clickedSlotKey]() {
    //         // Физически удаляем всю строку корневой секции из модели
    //         deviceModel->invisibleRootItem()->removeRow(ctx.targetSectionItem->row());

    //         // Если удалили слот, запускаем автоматический пересчет индексов шасси железа
    //         if (isSlot) {
    //             bool isBaseType = (QStringView(clickedSlotKey).mid(4).toInt() < 0);
    //             reindexSlotsOfType(isBaseType);
    //         }

    //         modified = true;
    //         emit onChanged();
    //     });
    // }
    // Запуск отображения контекстного меню
    menu.exec(deviceTreeView->viewport()->mapToGlobal(pos));
}

QJsonObject deviceView::createDefaultData(const QJsonObject &structureSchema)
{
    qDebug()<<structureSchema;
    QJsonObject defaultObj;

    for (auto it = structureSchema.begin(); it != structureSchema.end(); ++it) {
        QString key = it.key();
        QJsonObject paramMeta = it.value().toObject();
        QString type = paramMeta.value("type").toString();

        // 1. СЦЕНАРИЙ А: Универсальные динамические линки-диапазоны (линки каналов/регистров)
        if ((type == "link" || type == "keynum") && paramMeta.contains("structure")) {
            QJsonObject subStruct = paramMeta.value("structure").toObject();
            QJsonObject defaultSubData = createDefaultData(subStruct);

            // Читаем лимит из параметра "max" в схеме (если забыли указать, фоллбэк на 1)
            int maxChannels = paramMeta.contains("max") ? paramMeta.value("max").toInt() : 1;

            // Читаем флаг группировки в диапазон (например, true для in/out, false для chan)
            bool isRangeStyle = paramMeta.value("range").toBool(false);
            qDebug() << isRangeStyle << maxChannels;

            if (isRangeStyle) {
                // 1. ВАРИАНТ ДИАПАЗОНА (in0..15, out0..15)
                QString channelPostfix = QString("0..%1").arg(maxChannels - 1);
                QString rangeKey = key + channelPostfix;
                QJsonObject defaultSubData = createDefaultData(subStruct);
                applyVariablePostfix(defaultSubData, subStruct, channelPostfix);
                defaultObj.insert(rangeKey, defaultSubData);
            } else {
                // ОДНОТИПНО для любых поштучных параметров: разворачиваем плоскую сетку (chan0, chan1...)
                for (int c = 0; c < maxChannels; ++c) {
                    QString channelPostfix = QString::number(c);
                    QString individualKey = key + channelPostfix;
                    QJsonObject defaultSubData = createDefaultData(subStruct);
                    applyVariablePostfix(defaultSubData, subStruct, channelPostfix);
                    defaultObj.insert(individualKey, defaultSubData);
                }
            }
            continue;
        }

        // 2. СЦЕНАРИЙ Б: Вложенная фиксированная структурная группа (clock, rs485...)
        if (paramMeta.contains("structure")) {
            QJsonObject subStruct = paramMeta.value("structure").toObject();
            defaultObj.insert(key, createDefaultData(subStruct));
            continue;
        }

        // 3. СЦЕНАРИЙ В: Списковые параметры ("type": "sequence", например natural)
        if (type == "sequence") {
            QJsonArray defaultArray;
            if (paramMeta.contains("default")) {
                QJsonValue defVal = paramMeta.value("default");
                if (defVal.isArray()) {
                    defaultArray = defVal.toArray();
                } else if (defVal.isString() && !defVal.toString().isEmpty()) {
                    // Парсим массив, если он записан JSON-строкой в файле схемы
                    QJsonDocument arrDoc = QJsonDocument::fromJson(defVal.toString().toUtf8());
                    if (arrDoc.isArray()) defaultArray = arrDoc.array();
                }
            }
            // Никакого хардкода. Если дефолт не задан в схеме — массив остаётся пустым
            defaultObj.insert(key, defaultArray);
            continue;
        }

        // 4. СЦЕНАРИЙ Г: Обычный leaf-параметр (строка, число, enum)
        if (paramMeta.contains("default")) {
            // Пишем строго то значение по умолчанию, которое заложено в JSON-схему
            defaultObj.insert(key, paramMeta.value("default"));
        } else {
            // Если дефолт не задан — параметр считается необязательным.
            // Записываем пустую строку, оставляя ячейку пустой в интерфейсе QTreeView.
            defaultObj.insert(key, "");
        }
    }

    return defaultObj;
}


deviceView::ContextMenuContext deviceView::analyzeMenuContext(const QModelIndex &index)
{
    ContextMenuContext ctx;
    ctx.isValidClick = true;

    // Сценарий 1: Клик по пустому пространству
    if (!index.isValid()) {
        ctx.isBlankSpace = true;
        return ctx;
    }

    // Сценарий 2: Клик по существующим строкам
    QModelIndex nameIndex = index.siblingAtColumn(0);
    QStandardItem *nameItem = deviceModel->itemFromIndex(nameIndex);
    if (!nameItem) {
        ctx.isValidClick = false;
        return ctx;
    }

    // Безопасно вытаскиваем соседнюю ячейку Value на любом уровне вложенности дерева
    QStandardItem *valueItem = nullptr;
    if (nameItem->parent() == nullptr) {
        valueItem = deviceModel->item(nameItem->row(), 1);
    } else {
        valueItem = nameItem->parent()->child(nameItem->row(), 1);
    }

    QJsonObject meta = valueItem ? valueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();
    QString type = meta.value("type").toString();

    ctx.isSequenceMode = (type == "sequence");
    ctx.isAnyMode = meta.contains("any");

    qDebug() << nameItem ->text();
    qDebug() << meta;

    // =========================================================================
    // ОДНОКРАТНЫЙ И ЛИНЕЙНЫЙ АНАЛИЗ ИЕРАРХИИ СХЕМЫ
    // =========================================================================

    // Сценарий А: Кликнули на самый верхний КОРЕНЬ первого уровня (var, forte, slot3...)
    // У этих элементов физически НЕТ родителя в модели дерева!
    if (nameItem->parent() == nullptr) {
        ctx.targetSectionItem = nameItem;

        if (ctx.isAnyMode) {
            QJsonObject anyObj = meta.value("any").toObject();
            ctx.anyStruct = anyObj.value("structure").toObject();
            ctx.varDescription = anyObj.value("description").toString();
        }
        // Интегрируем логику защитного блока сюда: если это корень слота железа
        else if (nameItem->data(deviceView::SlotKeyRole).toString().startsWith("slot")) {
            if (meta.contains("structure")) {
                ctx.anyStruct = meta.value("structure").toObject();
            } else {
                QString modType = nameItem->data(deviceView::ModuleTypeRole).toString();
                ctx.anyStruct = findModuleSchema(modType).value("structure").toObject();
            }
        }
    }
    // Сценарий Б: Кликнули на ЛЮБОЙ дочерний элемент внутри (переменные, каналы, параметры)
    // У этих элементов гарантированно ЕСТЬ родитель в модели!
    else {
        qDebug() << "Сценарий Б: Кликнули на ЛЮБОЙ дочерний элемент внутри";
        ctx.isChildItem = true;
        ctx.currentItem = nameItem;

        // ПРОВЕРКА Б.1: Клик по самому групповому каналу/диапазону (out0..15, chan0, chan1)
        // Если элемент сам является линком — его схема параметров лежит в "structure"
        if (meta.contains("structure")) {
            qDebug() << "ПРОВЕРКА Б.1: Клик по самому групповому каналу";
            ctx.anyStruct = meta.value("structure").toObject();
            ctx.targetSectionItem = nameItem; // Фиксация: Цель вставки — сам узел канала ("chan0")!
            ctx.currentItem = nullptr;       // Очищаем, так как внутри этой папки мы еще ничего не выделили
        }
        // ПРОВЕРКА Б.2: Клик по конечному leaf-параметру внутри канала (holdtime, wires, mode)
        else {
            qDebug() << "ПРОВЕРКА Б.2: Клик по конечному leaf-параметру внутри канала";
            QStandardItem *parentNameItem = nameItem->parent();
            ctx.targetSectionItem = parentNameItem; // Цель вставки — папка-канал (родитель)

            QModelIndex parentNameIndex = parentNameItem->index();
            QModelIndex parentValueIndex = parentNameIndex.siblingAtColumn(1);
            QStandardItem *parentValueItem = deviceModel->itemFromIndex(parentValueIndex);

            QJsonObject parentMeta = parentValueItem ? parentValueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();

            if (parentMeta.contains("any")) {
                ctx.isAnyMode = true;
                QJsonObject anyObj = parentMeta.value("any").toObject();
                ctx.anyStruct = anyObj.value("structure").toObject();
                ctx.varDescription = anyObj.value("description").toString();
            } else if (parentMeta.value("type").toString() == "structure") {
                ctx.anyStruct = parentMeta.value("structure").toObject();
            } else if (parentMeta.value("type").toString() == "sequence") {
                ctx.isSequenceMode = true;
            } else if (parentMeta.contains("structure")) {
                ctx.anyStruct = parentMeta.value("structure").toObject();
            } else {
                // Если родитель — это корень слота (slot1)
                QString modType = parentNameItem->data(deviceView::ModuleTypeRole).toString();
                if (!modType.isEmpty()) {
                    ctx.anyStruct = findModuleSchema(modType).value("structure").toObject();
                }
            }
        }
    }

    qDebug() << "Финишируем с targetSectionItem:" << ctx.targetSectionItem->text();
    return ctx;
}

void deviceView::reindexSlotsOfType(bool isBaseType)
{
    int nextIdx = 1;       // Для IO: slot1, slot2, slot3...
    int nextBaseIdx = -1;  // Для Base: slot-1, slot-2, slot-3...

    // Сначала посчитаем, сколько всего базовых модулей у нас в модели,
    // чтобы правильно задать стартовый отрицательный индекс сверху вниз
    if (isBaseType) {
        int totalBase = 0;
        for (int i = 0; i < deviceModel->rowCount(); ++i) {
            QStandardItem *rootItem = deviceModel->item(i, 0);
            if (rootItem) {
                QString key = rootItem->data(deviceView::SlotKeyRole).toString();
                if (key.startsWith("slot") && QStringView(key).mid(4).toInt() < 0) {
                    totalBase++;
                }
            }
        }
        nextBaseIdx = -totalBase; // Если базовых модулей 3, то верхний получит -3, затем -2, -1
    }

    // Проходим по всей модели сверху вниз и обновляем индексы для выбранного типа
    for (int i = 0; i < deviceModel->rowCount(); ++i) {
        QStandardItem *rootItem = deviceModel->item(i, 0);
        if (!rootItem) continue;

        QString savedSlotKey = rootItem->data(deviceView::SlotKeyRole).toString();
        if (savedSlotKey.startsWith("slot")) {
            int slotNum = QStringView(savedSlotKey).mid(4).toInt();
            bool currentIsBase = (slotNum < 0);

            if (currentIsBase == isBaseType) {
                QString newSlotKey;
                if (isBaseType) {
                    newSlotKey = QString("slot%1").arg(nextBaseIdx++);
                } else {
                    newSlotKey = QString("slot%1").arg(nextIdx++);
                }

                // Извлекаем имя модуля из круглых скобок старого заголовка
                QString currentText = rootItem->text();
                QString modName = currentText.split("(").last().trimmed();
                if (modName.endsWith(")")) modName.chop(1);

                // Записываем обновленные данные в ячейку
                rootItem->setText(QString("%1 (%2)").arg(newSlotKey, modName));
                rootItem->setData(newSlotKey, deviceView::SlotKeyRole);
            }
        }
    }
}

void deviceView::applyVariablePostfix(QJsonObject &defaultSubData, const QJsonObject &subStruct, const QString &postfix)
{
    for (const QString &subKey : defaultSubData.keys()) {
        // Проверяем, является ли параметр ключом привязки переменной
        if (subKey == "var" || subKey == "var_out") {
            QJsonObject paramMeta = subStruct.value(subKey).toObject();

            // Если в схеме для него жестко задан default, модифицируем его
            if (paramMeta.contains("default")) {
                QString baseDefaultName = paramMeta.value("default").toString();
                if (!baseDefaultName.isEmpty()) {
                    defaultSubData.insert(subKey, baseDefaultName + postfix);
                }
            }
        }
    }
}
