#include "deviceview.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMenu>
#include "logmanager.h"

#include <QStyledItemDelegate>
#include <QComboBox>

class DeviceNodeDelegate : public QStyledItemDelegate
{
public:
    explicit DeviceNodeDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // Работаем только со второй колонкой (Value)
        if (index.column() != 1) {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }

        // Вытаскиваем метаданные, которые вы прикрепили к ячейке в parseSchemaNode
        QJsonObject meta = index.data(deviceView::SchemaMetaRole).toJsonObject();
        QString type = meta.value("type").toString();
        qDebug() << type << meta;

        // Если это enum — строим выпадающий список
        if (type == "enum") {
            QComboBox *comboBox = new QComboBox(parent);
            comboBox->setFrame(false);

            QJsonArray enumArray = meta.value("values").toArray();
            qDebug() << enumArray;
            for (const QJsonValue &val : enumArray) {
                if (val.isDouble()) {
                    comboBox->addItem(QString::number(val.toVariant().toLongLong()));
                } else {
                    comboBox->addItem(val.toString());
                }
            }
            return comboBox;
        }

        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox) {
            QString currentText = index.data(Qt::EditRole).toString();
            int cbIndex = comboBox->findText(currentText);
            if (cbIndex != -1) {
                comboBox->setCurrentIndex(cbIndex);
            }
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox) {
            model->setData(index, comboBox->currentText(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        editor->setGeometry(option.rect);
    }
};

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
    deviceTreeView->setItemDelegateForColumn(1, new DeviceNodeDelegate(this));

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
            if (!yamlObj.contains(key) || yamlObj.value(key).isUndefined() || yamlObj.value(key).isNull()) {
                continue;
            }

            QJsonObject anyMeta = meta.contains("any") ? meta.value("any").toObject() : meta;
            QJsonObject anyStruct = anyMeta.value("structure").toObject();

            // ЗАЩИТА ОТ ДУБЛИРОВАНИЯ: Ищем, нет ли уже на текущем уровне узла с таким именем
            QStandardItem *sectionNameItem = nullptr;

            for (int r = 0; r < parentNode->rowCount(); ++r) {
                if (parentNode->child(r, 0) && parentNode->child(r, 0)->text() == key) {
                    sectionNameItem = parentNode->child(r, 0);
                    break; // Нашли существующую секцию, её имя нам известно!
                }
            }

            // Если секции ещё нет в дереве, создаём её с нуля
            if (!sectionNameItem) {
                sectionNameItem = new QStandardItem(key);
                sectionNameItem->setEditable(false);

                QStandardItem *sectionValueItem = new QStandardItem("");
                sectionValueItem->setEditable(false);
                sectionValueItem->setData(meta, deviceView::SchemaMetaRole);

                QStandardItem *sectionDescItem = new QStandardItem(meta.value("description").toString());
                sectionDescItem->setEditable(false);

                // Добавляем саму секцию в модель
                parentNode->appendRow({sectionNameItem, sectionValueItem, sectionDescItem});
            }

            // Б) Извлекаем объект данных из YAML именно для этой секции
            QJsonObject anyDataObj = yamlObj.value(key).toObject();

            // В) Идем по реальным переменным пользователя и складываем их ВНУТРЬ sectionNameItem
            for (auto yIt = anyDataObj.begin(); yIt != anyDataObj.end(); ++yIt) {
                // Дополнительная защита: проверяем, нет ли уже внутри секции переменной с таким именем
                bool varExists = false;
                for (int ch = 0; ch < sectionNameItem->rowCount(); ++ch) {
                    if (sectionNameItem->child(ch, 0) && sectionNameItem->child(ch, 0)->text() == yIt.key()) {
                        varExists = true;
                        break;
                    }
                }
                if (varExists) continue; // Не дублируем уже существующие переменные проекта

                QStandardItem *varNameItem = new QStandardItem(yIt.key());
                varNameItem->setEditable(false);

                QStandardItem *varValueItem = new QStandardItem("");
                varValueItem->setEditable(false);

                QStandardItem *varDescItem = new QStandardItem("");
                varDescItem->setEditable(false);

                // ВАЖНО: Добавляем строку в найденную или созданную секцию
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
    // qDebug()<< "deviceView::resetModified modified = false";
    modified = false;
}

QStringList deviceView::getAllProjectVariables() const
{
    QStringList variableNames;
    if (!deviceModel) return variableNames;

    // Ищем на верхнем уровне модели корневую папку "var"
    QStandardItem *varSectionItem = nullptr;
    for (int i = 0; i < deviceModel->rowCount(); ++i) {
        if (deviceModel->item(i, 0) && deviceModel->item(i, 0)->text() == "var") {
            varSectionItem = deviceModel->item(i, 0);
            break;
        }
    }

    // Если папка найдена, забираем имена всех её дочерних строк
    if (varSectionItem) {
        for (int j = 0; j < varSectionItem->rowCount(); ++j) {
            if (varSectionItem->child(j, 0)) {
                variableNames.append(varSectionItem->child(j, 0)->text());
            }
        }
    }

    return variableNames;
}


void deviceView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    modified = true;
    emit onChanged();
}

void deviceView::showContextMenu(const QPoint &pos)
{
    QModelIndex index = deviceTreeView->indexAt(pos);

    // 1. ЭТАП АНАЛИЗА: Получаем чистый контекст клика
    ContextMenuContext ctx = analyzeMenuContext(index);
    if (!ctx.isValidClick) return;

    QMenu menu(this);

    // =========================================================================
    // ЗОНА А: НАПОЛНЕНИЕ ДЛЯ ПУСТОГО ПРОСТРАНСТВА (Глобальное конструирование ПЛК)
    // =========================================================================
    if (ctx.isBlankSpace) {
        QStringList existingRootItems;
        int currentBaseCount = 0;
        int currentIoCount = 0;

        int minBaseIdx = 0; // Для отрицательных индексов слотов базовых модулей (-1, -2)
        int maxIoIdx = 0;   // Для положительных индексов слотов ввода-вывода (1, 2)

        // 1. АНАЛИЗИРУЕМ ТЕКУЩУЮ ТОПОЛОГИЮ ШАССИ
        for (int i = 0; i < deviceModel->rowCount(); ++i) {
            QStandardItem *rootItem = deviceModel->item(i, 0);
            if (!rootItem) continue;

            QString name = rootItem->text();
            existingRootItems.append(name.contains("slot") ? "slot" : name);

            QString savedSlotKey = rootItem->data(deviceView::SlotKeyRole).toString();
            if (savedSlotKey.startsWith("slot")) {
                int slotNum = QStringView(savedSlotKey).mid(4).toInt();

                if (slotNum < 0) {
                    currentBaseCount++;
                    if (slotNum < minBaseIdx) minBaseIdx = slotNum;
                } else {
                    currentIoCount++;
                    if (slotNum > maxIoIdx) maxIoIdx = slotNum;
                }
            }
        }

        QStringList missingRootBlocks;
        bool hasSlotSpecification = m_schemaRoot.contains("slot");
        for (auto it = m_schemaRoot.begin(); it != m_schemaRoot.end(); ++it) {
            if (it.key() != "slot" && !existingRootItems.contains(it.key())) {
                missingRootBlocks.append(it.key());
            }
        }

        // Выводим меню для обычных пропущенных блоков (clock, ipaddr)
        if (!missingRootBlocks.isEmpty()) {
            QMenu *addBlockMenu = menu.addMenu(tr("Добавить блок конфигурации"));
            for (const QString &blockKey : missingRootBlocks) {
                QJsonObject blockMeta = m_schemaRoot.value(blockKey).toObject();
                QString blockDesc = blockMeta.value("description").toString();

                addBlockMenu->addAction(blockKey, this, [this, blockKey, blockMeta, blockDesc]() {
                    QJsonObject defaultData;
                    QJsonObject subSchema = blockMeta.contains("structure") ? blockMeta.value("structure").toObject() : m_schemaRoot;
                    if (blockMeta.contains("structure")) {
                        defaultData.insert(blockKey, createDefaultData(subSchema));
                        parseSchemaNode(deviceModel->invisibleRootItem(), m_schemaRoot, QJsonValue(defaultData));
                    } else {
                        QJsonObject singleWrapperSchema;
                        singleWrapperSchema.insert(blockKey, blockMeta);

                        QJsonObject generatedLeafData = createDefaultData(singleWrapperSchema, true);
                        QString finalDefaultValue = generatedLeafData.value(blockKey).toString();

                        QStandardItem *pName = new QStandardItem(blockKey);
                        pName->setEditable(false);

                        // Вместо жестких пустых кавычек "" подставляем вычитанное фабричное значение!
                        QStandardItem *pValue = new QStandardItem(finalDefaultValue);
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

        // Выводим меню для добавления новых плат в слоты шасси железа
        if (hasSlotSpecification) {
            QMenu *addSlotMenu = menu.addMenu(tr("Добавить модуль в новый слот"));
            QJsonObject slotMeta = m_schemaRoot.value("slot").toObject();
            QJsonArray availableModules = slotMeta.value("modules").toArray();

            for (const QJsonValue &modVal : availableModules) {
                QJsonObject modObj = modVal.toObject();
                QString modName = modObj.value("name").toString();
                QString modDesc = modObj.value("description").toString();
                QJsonObject modStructure = modObj.value("structure").toObject();

                // Декларативно вытаскиваем лимиты из параметров схемы модулей
                bool isBaseModule = modObj.contains("base");
                int maxAllowed = isBaseModule ? modObj.value("base").toInt() : modObj.value("io").toInt();
                int currentCount = isBaseModule ? currentBaseCount : currentIoCount;

                QAction *modAction = new QAction(modName, this);

                // КОНТРОЛЬ ЛИМИТОВ ШАССИ: Если платы превысили лимит САПР, блокируем клик
                if (currentCount >= maxAllowed) {
                    modAction->setEnabled(false);
                    modAction->setText(QString("%1 [%2]").arg(modAction->text(), tr("лимит")));
                }
                addSlotMenu->addAction(modAction);

                connect(modAction, &QAction::triggered, this, [this, modStructure, modName, modDesc, isBaseModule, minBaseIdx, maxIoIdx]() {
                    // Вычисляем имя и SlotKeyRole на основе топологии
                    QString newSlotKey = isBaseModule ? QString("slot%1").arg(minBaseIdx - 1) : QString("slot%1").arg(maxIoIdx + 1);

                    QStandardItem *slotNameItem = new QStandardItem(QString("%1 (%2)").arg(newSlotKey, modName));
                    slotNameItem->setEditable(false);
                    slotNameItem->setData(newSlotKey, deviceView::SlotKeyRole);

                    QString rawModuleType = modStructure.value("module").toObject().value("value").toString();
                    if (rawModuleType.isEmpty() && modStructure.value("module").toObject().value("value").isArray()) {
                        rawModuleType = modStructure.value("module").toObject().value("value").toArray().at(0).toString();
                    }
                    slotNameItem->setData(rawModuleType, deviceView::ModuleTypeRole);

                    QStandardItem *slotValueItem = new QStandardItem("");
                    slotValueItem->setEditable(false);

                    QStandardItem *slotDescItem = new QStandardItem(modDesc);
                    slotDescItem->setEditable(false);

                    // =========================================================================
                    // УМНОЕ ПОЗИЦИОНИРОВАНИЕ НА ШАССИ (Base - вверх, IO - вниз)
                    // =========================================================================
                    int firstAnySlotRow = -1;
                    int lastAnySlotRow = -1;
                    for (int r = 0; r < deviceModel->rowCount(); ++r) {
                        QStandardItem *item = deviceModel->item(r, 0);
                        if (item && item->data(deviceView::SlotKeyRole).toString().startsWith("slot")) {
                            if (firstAnySlotRow == -1) firstAnySlotRow = r;
                            lastAnySlotRow = r;
                        }
                    }

                    int insertRowIdx = deviceModel->rowCount();
                    if (isBaseModule) {
                        // Базовые модули нарастают вверх: занимают место самого первого слота, сдвигая IO вниз
                        insertRowIdx = (firstAnySlotRow != -1) ? firstAnySlotRow : 0;
                    } else {
                        // Модули ввода-вывода (IO) складываются строго вниз: под самый последний слот
                        insertRowIdx = (lastAnySlotRow != -1) ? lastAnySlotRow + 1 : 0;
                    }

                    // Физически добавляем строку на шасси в правильную позицию
                    deviceModel->invisibleRootItem()->insertRow(insertRowIdx, {slotNameItem, slotValueItem, slotDescItem});

                    // Переиндексируем текстовые имена заголовков по порядку
                    reindexSlotsOfType(isBaseModule);

                    QJsonObject defaultSlotData = createDefaultData(modStructure, false);
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
        return;
    }
    // =========================================================================
    // ЗОНА Б: НАПОЛНЕНИЕ ДЛЯ СУЩЕСТВУЮЩИХ СТРОК ДЕРЕВА (Строгая иерархия if-else)
    // =========================================================================
    else {
        QStandardItem *parentContainer = ctx.targetSectionItem->parent() ? ctx.targetSectionItem->parent() : ctx.targetSectionItem;
        bool isRootNode = !ctx.targetSectionItem->parent();

        // -------------------------------------------------------------------------
        // СЦЕНАРИЙ 1: Кликнули по КОРНЮ СЛОТА ЖЕЛЕЗА на самом верхнем уровне (slot-1, slot1...)
        // -------------------------------------------------------------------------
        // Проверяем роль SlotKeyRole прямо на месте, убирая варнинг clazy-unused
        if (isRootNode && parentContainer->data(deviceView::SlotKeyRole).toString().startsWith("slot")) {
            QString slotKey = parentContainer->data(deviceView::SlotKeyRole).toString();
            int slotNum = QStringView(slotKey).mid(4).toInt();
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

            menu.addAction(tr("Удалить модуль со слота"), this, [this, ctx, isBaseType]() {
                deviceModel->invisibleRootItem()->removeRow(ctx.targetSectionItem->row());
                reindexSlotsOfType(isBaseType);
                modified = true; emit onChanged();
            });

            if (ctx.allowRestoreSchemaParams && !ctx.anyStruct.isEmpty()) {
                menu.addSeparator();
                buildRestoreMenu(&menu, ctx.targetSectionItem, ctx.anyStruct);
            }
        }
        // -------------------------------------------------------------------------
        // СЦЕНАРИЙ 2: Кликнули по КОРНЮ глобальной секции "var" (Переменные проекта)
        // -------------------------------------------------------------------------
        else if (isRootNode && ctx.isAnyMode) {
            // ФИКС: Возвращаем заветную кнопку добавления переменной!
            menu.addAction(tr("Добавить переменную"), this, [this, parentContainer, ctx]() {
                insertAndEditNewRow(parentContainer, ctx.varDescription, [this, ctx](QStandardItem* insertedNode) {
                    parseSchemaNode(insertedNode, ctx.anyStruct, QJsonValue(createDefaultData(ctx.anyStruct)));
                });
            });

            menu.addAction(tr("Удалить блок конфигурации"), this, [this, ctx]() {
                deviceModel->invisibleRootItem()->removeRow(ctx.targetSectionItem->row());
                modified = true; emit onChanged();
            });
        }
        // -------------------------------------------------------------------------
        // СЦЕНАРИЙ 3: Кликнули по КОРНЮ ОБЫЧНОГО СТАТИЧЕСКОГО БЛОКА (forte, clock)
        // -------------------------------------------------------------------------
        else if (isRootNode) {
            menu.addAction(tr("Удалить блок конфигурации"), this, [this, ctx]() {
                deviceModel->invisibleRootItem()->removeRow(ctx.targetSectionItem->row());
                modified = true; emit onChanged();
            });

            if (ctx.isSequenceMode && ctx.allowAddSequenceItem) {
                menu.addSeparator();
                menu.addAction("Добавить элемент списка", this, [this, ctx]() { insertAndEditNewRow(ctx.targetSectionItem); });
            }

            if (ctx.allowRestoreSchemaParams && !ctx.anyStruct.isEmpty()) {
                menu.addSeparator();
                buildRestoreMenu(&menu, ctx.targetSectionItem, ctx.anyStruct);
            }
        }
        // -------------------------------------------------------------------------
        // СЦЕНАРИЙ 4: Управление элементами списков массивов SEQUENCE (Теги Forte)
        // -------------------------------------------------------------------------
        else if (ctx.isSequenceMode) {
            if (ctx.allowAddSequenceItem) {
                menu.addAction("Добавить элемент списка", this, [this, ctx]() {
                    insertAndEditNewRow(ctx.targetSectionItem);
                });
            } else if (ctx.allowDeleteSequenceItem) {
                menu.addAction("Удалить элемент списка", this, [this, parentContainer, ctx]() {
                    parentContainer->removeRow(ctx.targetSectionItem->row());
                    modified = true;
                    emit onChanged();
                });
            }
        }
        // 5. УПРАВЛЕНИЕ ВЛОЖЕННЫМИ ПАРАМЕТРАМИ, ПЕРЕМЕННЫМИ И КАНАЛАМИ ПЛАТ РАСШИРЕНИЯ
        else {
            // А) Кнопка удаления (Для любых leaf-параметров, переменных var или каналов)
            if (ctx.allowDeleteVariableOrParam) {
                QString delText = ctx.isAnyMode ? tr("Удалить переменную") : tr("Удалить параметр");
                if (ctx.targetSectionItem->hasChildren()) delText = tr("Удалить канал/группу");

                menu.addAction(delText, this, [this, parentContainer, ctx]() {
                    parentContainer->removeRow(ctx.targetSectionItem->row());
                    modified = true;
                    emit onChanged();
                });
                menu.addSeparator();
            }

            // Б) Блок быстрого интерактивного экспорта переменных в Forte
            if (ctx.allowVariableExportToForte) {
                QString varName = ctx.currentItemText;
                QStandardItem *forteSection = nullptr;
                for (int j = 0; j < deviceModel->rowCount(); ++j) {
                    if (deviceModel->item(j, 0) && deviceModel->item(j, 0)->text() == "forte") {
                        forteSection = deviceModel->item(j, 0);
                        break;
                    }
                }

                if (forteSection) {
                    auto tryAddVarToForteArray = [this, varName](QStandardItem* subSec, const QString& txt) -> QAction* {
                        if (!subSec) return nullptr;
                        for (int j = 0; j < subSec->rowCount(); ++j) {
                            if (subSec->child(j, 0) && subSec->child(j, 0)->text() == varName) return nullptr;
                        }
                        return new QAction(txt, this);
                    };

                    QStandardItem *fVar = nullptr;
                    QStandardItem *fVarOut = nullptr;
                    for (int j = 0; j < forteSection->rowCount(); ++j) {
                        if (forteSection->child(j, 0)->text() == "var") fVar = forteSection->child(j, 0);
                        if (forteSection->child(j, 0)->text() == "var_out") fVarOut = forteSection->child(j, 0);
                    }

                    QAction *actVar = tryAddVarToForteArray(fVar, "Добавить в forte/var");
                    QAction *actVarOut = tryAddVarToForteArray(fVarOut, "Добавить в forte/var_out");

                    if (actVar) {
                        menu.addAction(actVar);
                        connect(actVar, &QAction::triggered, this, [this, fVar, varName]() {
                            fVar->insertRow(0, {new QStandardItem(varName), new QStandardItem(), new QStandardItem()});
                            deviceTreeView->expand(fVar->index());
                            modified = true;
                            emit onChanged();
                        });
                    }
                    if (actVarOut) {
                        menu.addAction(actVarOut);
                        connect(actVarOut, &QAction::triggered, this, [this, fVarOut, varName]() {
                            fVarOut->insertRow(0, {new QStandardItem(varName), new QStandardItem(), new QStandardItem()});
                            deviceTreeView->expand(fVarOut->index());
                            modified = true;
                            emit onChanged();
                        });
                    }
                    if (actVar || actVarOut) menu.addSeparator();
                }
            }

            // В) Выводим строго ОДНО подменю "Добавить..." для восстановления стёртых дочерних полей/каналов
            if (ctx.allowRestoreSchemaParams && !ctx.anyStruct.isEmpty()) {
                buildRestoreMenu(&menu, ctx.targetSectionItem, ctx.anyStruct);
            }
        }
    }

    // 8. Общие пункты развёртывания дерева
    if (!menu.isEmpty()) {
        menu.addSeparator();
        menu.addAction(tr("Развернуть всё"), deviceTreeView, &QTreeView::expandAll);
        menu.addAction(tr("Свернуть всё"), deviceTreeView, &QTreeView::collapseAll);
        menu.exec(deviceTreeView->viewport()->mapToGlobal(pos));
    }
}

QJsonObject deviceView::createDefaultData(const QJsonObject &structureSchema, bool forceCreateAll)
{
    QJsonObject defaultObj;

    for (auto it = structureSchema.begin(); it != structureSchema.end(); ++it) {
        QString key = it.key();
        QJsonObject paramMeta = it.value().toObject();
        QString type = paramMeta.value("type").toString();

        // 1. СЦЕНАРИЙ А: Универсальные динамические линки-диапазоны (chan, out и т.д.)
        if ((type == "link" || type == "keynum") && paramMeta.contains("structure")) {
            // Если мы просто генерируем базовое шасси/модуль, не создаем вложенные бесконечные коллекции/регистры автоматически,
            // кроме тех случаев, когда это форсировано из меню восстановления
            if (!forceCreateAll && !paramMeta.contains("default")) {
                continue;
            }

            QJsonObject subStruct = paramMeta.value("structure").toObject();
            int maxChannels = paramMeta.contains("max") ? paramMeta.value("max").toInt() : 1;
            bool isRangeStyle = paramMeta.value("range").toBool(false);

            if (isRangeStyle) {
                QString channelPostfix = QString("0..%1").arg(maxChannels - 1);
                QString rangeKey = key + channelPostfix;

                // Передаем forceCreateAll = false, чтобы внутренние параметры фильтровались по наличию "default"
                QJsonObject defaultSubData = createDefaultData(subStruct, false);
                applyVariablePostfix(defaultSubData, subStruct, channelPostfix);
                defaultObj.insert(rangeKey, defaultSubData);
            } else {
                int channelsToCreate = paramMeta.contains("max") ? maxChannels : 1;
                for (int c = 0; c < channelsToCreate; ++c) {
                    QString channelPostfix = QString::number(c);
                    QString individualKey = key;
                    if (!key.isEmpty() && !key.at(key.length() - 1).isDigit()) {
                        individualKey = key + channelPostfix;
                    }

                    QJsonObject defaultSubData = createDefaultData(subStruct, false);
                    applyVariablePostfix(defaultSubData, subStruct, channelPostfix);
                    defaultObj.insert(individualKey, defaultSubData);
                }
            }
            continue;
        }

        // 2. СЦЕНАРИЙ Б: Вложенная фиксированная структурная группа (clock, rs485, modbus_server...)
        if (paramMeta.contains("structure")) {
            QJsonObject subStruct = paramMeta.value("structure").toObject();

            // При уходе вглубь фиксированной структуры, мы собираем только дефолтные параметры (forceCreateAll = false)
            QJsonObject subData = createDefaultData(subStruct, false);

            if (!subData.isEmpty() || forceCreateAll) {
                defaultObj.insert(key, subData);
            }
            continue;
        }

        // 3. СЦЕНАРИЙ В: Списковые параметры ("type": "sequence")
        if (type == "sequence") {
            if (paramMeta.contains("default") || forceCreateAll) {
                QJsonArray defaultArray;
                QJsonValue defVal = paramMeta.value("default");
                if (defVal.isArray()) {
                    defaultArray = defVal.toArray();
                } else if (defVal.isString() && !defVal.toString().isEmpty()) {
                    QJsonDocument arrDoc = QJsonDocument::fromJson(defVal.toString().toUtf8());
                    if (arrDoc.isArray()) defaultArray = arrDoc.array();
                }
                defaultObj.insert(key, defaultArray);
            }
            continue;
        }

        // 4. СЦЕНАРИЙ Г: Обычный конечный leaf-параметр (строка, число, enum)
        // ГЛАВНОЕ ИЗМЕНЕНИЕ: Если флага forceCreateAll нет, мы добавляем параметр ТОЛЬКО при наличии "default" в схеме.
        // Это предотвратит автоматическое создание var_out, order, var_trigger и т.д.
        if (paramMeta.contains("default")) {
            defaultObj.insert(key, paramMeta.value("default"));
        }
        else if (forceCreateAll) {
            // Этот блок сработает исключительно тогда, когда пользователь ОСОЗНАННО
            // выбрал пункт конкретного параметра в контекстном подменю "Добавить..."
            defaultObj.insert(key, "");
        }
    }

    return defaultObj;
}



deviceView::ContextMenuContext deviceView::analyzeMenuContext(const QModelIndex &index)
{
    ContextMenuContext ctx;
    ctx.isValidClick = true;

    if (!index.isValid()) {
        ctx.isBlankSpace = true;
        return ctx;
    }

    QModelIndex nameIndex = index.siblingAtColumn(0);
    QStandardItem *nameItem = deviceModel->itemFromIndex(nameIndex);
    if (!nameItem) {
        ctx.isValidClick = false;
        return ctx;
    }

    ctx.targetSectionItem = nameItem;
    ctx.currentItemText = nameItem->text();
    ctx.parentContainer = nameItem->parent() ? nameItem->parent() : nameItem;

    QStandardItem *valueItem = nameItem->parent() ? nameItem->parent()->child(nameItem->row(), 1) : deviceModel->item(nameItem->row(), 1);

    QJsonObject meta = valueItem ? valueItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();
    QString type = meta.value("type").toString();

    ctx.isSequenceMode = (type == "sequence");
    ctx.isAnyMode = meta.contains("any");

    // Сценарий А: Клик по корневым узлам ПЕРВОГО уровня (у которых нет родителя)
    if (nameItem->parent() == nullptr) {
        QString slotKey = nameItem->data(deviceView::SlotKeyRole).toString();

        // 1. Если это слот шасси железа (slot-1, slot1...)
        if (slotKey.startsWith("slot")) {
            ctx.showSlotManagement = true;
            if (meta.contains("structure")) {
                ctx.anyStruct = meta.value("structure").toObject();
            } else {
                QString modType = nameItem->data(deviceView::ModuleTypeRole).toString();
                ctx.anyStruct = findModuleSchema(modType).value("structure").toObject();
            }
            ctx.allowRestoreSchemaParams = true;
        }
        // 2. Если это обычный статический корневой блок (forte, clock)
        else {
            ctx.showRootBlockDelete = true;

            if (meta.contains("structure")) {
                ctx.anyStruct = meta.value("structure").toObject();
                ctx.allowRestoreSchemaParams = true; // Теперь стёртый "enable" вернётся в один клик!
            }
        }

        // 3. Если это глобальная секция переменных проекта (var)
        if (ctx.isAnyMode) {
            ctx.allowAddGlobalVariable = true;
            QJsonObject anyObj = meta.value("any").toObject();
            ctx.anyStruct = anyObj.value("structure").toObject();
            ctx.varDescription = anyObj.value("description").toString();
            // Для корня var поиск скрытых параметров (init/retain) запрещен, они ищутся внутри самих переменных
            ctx.allowRestoreSchemaParams = false;
        }

        if (ctx.isSequenceMode) {
            ctx.allowAddSequenceItem = true;
        }
    }

    // Сценарий Б: Клик по любым вложенным дочерним элементам
    else {
        QStandardItem *parentItem = nameItem->parent();
        QModelIndex pValIdx = parentItem->index().siblingAtColumn(1);
        QStandardItem *pValItem = deviceModel->itemFromIndex(pValIdx);

        // ИСПРАВЛЕНО: Явное приведение типов .value<QJsonObject>()
        QJsonObject parentMeta = pValItem ? pValItem->data(deviceView::SchemaMetaRole).value<QJsonObject>() : QJsonObject();

        // Б.1. Папка встроенного массива Forte (var / var_out внутри блока forte)
        if (ctx.isSequenceMode && parentItem->text() == "forte") {
            ctx.allowAddSequenceItem = true;
        }
        // Б.2. Элемент внутри массива Forte (например, конкретный тег)
        else if (parentMeta.value("type").toString() == "sequence" && parentItem->text() != "forte") {
            ctx.isSequenceMode = true;
            ctx.allowDeleteSequenceItem = true;
        }
        else {
            if (nameItem->text() != "module") {
                ctx.allowDeleteVariableOrParam = true;
            }

            // Клик по конкретной переменной var проекта
            if (parentMeta.contains("any")) {
                ctx.isAnyMode = true;
                QJsonObject anyObj = parentMeta.value("any").toObject();
                ctx.anyStruct = anyObj.value("structure").toObject();
                ctx.varDescription = anyObj.value("description").toString();

                ctx.allowVariableExportToForte = true;
                ctx.allowRestoreSchemaParams = true;
            }
            // Клик по групповому каналу/диапазону или вложенной структуре (out0..15, chan0, modbus_server)
            else if (meta.contains("structure")) {
                ctx.anyStruct = meta.value("structure").toObject();
                ctx.allowRestoreSchemaParams = true;
            }
            // Клик по плоскому leaf-параметру (macaddr, wires, enable, holdtime, hostname)
            else {
                if (parentMeta.value("type").toString() == "structure") {
                    ctx.anyStruct = parentMeta.value("structure").toObject();
                } else if (parentMeta.contains("structure")) {
                    ctx.anyStruct = parentMeta.value("structure").toObject();
                } else {
                    QString modType = parentItem->data(deviceView::ModuleTypeRole).toString();
                    if (!modType.isEmpty()) ctx.anyStruct = findModuleSchema(modType).value("structure").toObject();
                }

                ctx.isAnyMode = false;
                ctx.isSequenceMode = false;
                ctx.allowRestoreSchemaParams = false; // Полностью блокируем подменю "Добавить..." для плоских leaf-параметров
            }
        }
    }

    qDebug() << "Финишируем с targetSectionItem:" << ctx.targetSectionItem->text();
    qDebug() << meta;
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

void deviceView::buildRestoreMenu(QMenu *parentMenu, QStandardItem *menuTargetItem, const QJsonObject &activeStruct)
{
    if (!parentMenu || !menuTargetItem || activeStruct.isEmpty()) return;

    QStringList existingItems;
    for (int j = 0; j < menuTargetItem->rowCount(); ++j) {
        if (menuTargetItem->child(j, 0)) existingItems.append(menuTargetItem->child(j, 0)->text());
    }

    QStringList expandedUiItems;
    for (const QString &uiKey : existingItems) {
        expandedUiItems.append(lbyaml::expandVar(uiKey, nullptr));
    }

    QStringList missingFixedParams;
    struct MissingLinkOption {
        QString menuLabel; QString insertKey; QJsonObject linkMeta;
    };
    QList<MissingLinkOption> missingLinkOptions;

    // Справочник для хранения метаданных динамически сгенерированных ключей (modbus_client0 -> оригинальная схема)
    QJsonObject dynamicKeysMetaRegistry;

    for (auto it = activeStruct.begin(); it != activeStruct.end(); ++it) {
        QString schemaKey = it.key(); // Например, "modbus_client", "chan", "out"
        QJsonObject paramMeta = it.value().toObject();
        QString pType = paramMeta.value("type").toString();

        // Читаем наше новое декларативное свойство из схемы!
        bool isDynamicCollection = paramMeta.value("dynamic").toBool(false);

        // if ((pType == "link" || pType == "keynum") && paramMeta.contains("structure")) {
        //     bool isRangeStyle = paramMeta.value("range").toBool(false);

        //     // ПРОВЕРКА НА БЕСКОНЕЧНЫЙ ЛИНК (У которого в схеме отсутствует "max", например modbus_client)
        //     if (!paramMeta.contains("max")) {
        //         int maxExistingIdx = -1;
        //         bool hasAnyClient = false;

        //         // Пробегаем по UI и ищем максимальный занятый индекс для этого префикса
        //         for (const QString &uiKey : existingItems) {
        //             if (uiKey.startsWith(schemaKey)) {
        //                 hasAnyClient = true;
        //                 int num = QStringView(uiKey).mid(schemaKey.length()).toInt();
        //                 if (num > maxExistingIdx) maxExistingIdx = num;
        //             }
        //         }

        //         // Вычисляем следующий свободный индекс (если клиентов еще нет - будет 0, если есть 0 - будет 1, и т.д.)
        //         int nextFreeIdx = hasAnyClient ? (maxExistingIdx + 1) : 0;
        //         QString nextClientKey = schemaKey + QString::number(nextFreeIdx); // "modbus_client1"
        //         if (isDynamicCollection) {
        //             missingFixedParams.append(nextClientKey);
        //             dynamicKeysMetaRegistry.insert(nextClientKey, paramMeta);
        //         } else {
        //             // Все остальные бесконечные keynum-регистры (holding, inreg) по-прежнему идут в Ветку Б
        //             MissingLinkOption opt;
        //             opt.menuLabel = nextClientKey;
        //             opt.insertKey = nextClientKey;
        //             opt.linkMeta = paramMeta;
        //             missingLinkOptions.append(opt);
        //         }
        //     }
        // ПРОВЕРКА НА БЕСКОНЕЧНЫЙ ЛИНК (У которого в схеме отсутствует "max", например modbus_client, holding)
        if ((pType == "link" || pType == "keynum") && paramMeta.contains("structure")) {
            bool isRangeStyle = paramMeta.value("range").toBool(false);

            if (!paramMeta.contains("max")) {
                int maxExistingIdx = -1;
                bool hasAnyItems = false;

                // Пробегаем по UI элементам текущего уровня, чтобы найти максимальный занятый индекс.
                // Учитываем особенности хранения: у keynum индекс лежит в Value (вторая колонка),
                // а в Parameter лежит чистый префикс ("holding").
                for (int r = 0; r < menuTargetItem->rowCount(); ++r) {
                    QStandardItem *uiNameItem = menuTargetItem->child(r, 0);
                    QStandardItem *uiValueItem = menuTargetItem->child(r, 1);
                    if (!uiNameItem) continue;

                    QString uiKey = uiNameItem->text();

                    // Сценарий 1: это существующий keynum (в Parameter имя типа "holding")
                    if (pType == "keynum" && uiKey == schemaKey && uiValueItem) {
                        hasAnyItems = true;
                        QString valStr = uiValueItem->text(); // Там может быть "1" или диапазон "10..15"

                        int dotIdx = valStr.indexOf("..");
                        if (dotIdx != -1) {
                            valStr = valStr.mid(dotIdx + 2); // Берем правую границу диапазона (например, "15")
                        }

                        int num = valStr.toInt();
                        if (num > maxExistingIdx) maxExistingIdx = num;
                    }
                    // Сценарий 2: это стандартный link (в Parameter имя типа "modbus_client1")
                    else if (pType != "keynum" && uiKey.startsWith(schemaKey)) {
                        hasAnyItems = true;
                        int num = QStringView(uiKey).mid(schemaKey.length()).toInt();
                        if (num > maxExistingIdx) maxExistingIdx = num;
                    }
                }

                // Вычисляем следующий свободный индекс
                int nextFreeIdx = hasAnyItems ? (maxExistingIdx + 1) : 0;

                if (pType == "keynum") {
                    // КРИТИЧЕСКИЙ ФИКС ДЛЯ HOLDING:
                    // Для keynum ключ в JSON схеме должен оставаться чистым базовым префиксом ("holding").
                    // А вычисленный номер мы передаем как отображаемый лейбл и суффикс для постобработки.
                    MissingLinkOption opt;
                    opt.menuLabel = QString("%1 [%2]").arg(schemaKey).arg(nextFreeIdx); // В меню будет: holding [16]
                    opt.insertKey = schemaKey; // Важно! Оставляем "holding", чтобы parseSchemaNode распознал тип keynum
                    opt.linkMeta = paramMeta;

                    // Чтобы передать вычисленный номер в логику генерации дефолтов, временно сохраним его
                    // внутри структуры через кастомное поле (метод parseSchemaNode это проигнорирует)
                    opt.linkMeta.insert("_calculated_idx", QString::number(nextFreeIdx));
                    missingLinkOptions.append(opt);
                }
                else {
                    // Стандартное поведение для динамических коллекций типа "modbus_client"
                    QString nextClientKey = schemaKey + QString::number(nextFreeIdx);
                    if (isDynamicCollection) {
                        missingFixedParams.append(nextClientKey);
                        dynamicKeysMetaRegistry.insert(nextClientKey, paramMeta);
                    } else {
                        MissingLinkOption opt;
                        opt.menuLabel = nextClientKey;
                        opt.insertKey = nextClientKey;
                        opt.linkMeta = paramMeta;
                        missingLinkOptions.append(opt);
                    }
                }
                continue; // Переходим к следующему элементу схемы, этот уже обработан
            }
            // КАНАЛЫ С ФИКСИРОВАННЫМ ЛИМИТОМ ИЗ СХЕМЫ (chan0..3, out0..15)
            else {
                int maxChannels = paramMeta.value("max").toInt();

                QStringList missingIndividualChannels;
                for (int c = 0; c < maxChannels; ++c) {
                    QString individualKey = QString("%1%2").arg(schemaKey).arg(c);
                    if (!expandedUiItems.contains(individualKey) && !existingItems.contains(individualKey)) {
                        missingIndividualChannels.append(individualKey);
                    }
                }

                if (isRangeStyle && missingIndividualChannels.size() == maxChannels) {
                    QString rangeKey = schemaKey + "0.." + QString::number(maxChannels - 1);
                    MissingLinkOption opt;
                    opt.menuLabel = rangeKey + tr(" (Весь диапазон)");
                    opt.insertKey = rangeKey; opt.linkMeta = paramMeta;
                    missingLinkOptions.append(opt);
                }

                for (const QString &missingChan : missingIndividualChannels) {
                    MissingLinkOption opt;
                    opt.menuLabel = missingChan; opt.insertKey = missingChan; opt.linkMeta = paramMeta;
                    missingLinkOptions.append(opt);
                }
            }
        } else {
            if (!existingItems.contains(schemaKey) && schemaKey != "module") missingFixedParams.append(schemaKey);
        }
    }

    if (!missingFixedParams.isEmpty() || !missingLinkOptions.isEmpty()) {
        QMenu *addMissingMenu = parentMenu->addMenu(tr("Добавить..."));
        // А) Восстановление фиксированных свойств (init, holdtime, wires, modbus_server...)
        for (const QString &missingKey : missingFixedParams) {
            addMissingMenu->addAction(missingKey, this, [this, menuTargetItem, missingKey, activeStruct, dynamicKeysMetaRegistry]() {
                QJsonObject singleSchema;
                // Проверяем, откуда брать метаданные схемы: из оригинальной структуры или из реестра динамических ключей
                if (dynamicKeysMetaRegistry.contains(missingKey)) {
                    singleSchema.insert(missingKey, dynamicKeysMetaRegistry.value(missingKey).toObject());
                } else {
                    singleSchema.insert(missingKey, activeStruct.value(missingKey).toObject());
                }
                qDebug() << "buildRestoreMenu" << "А) Восстановление фиксированных свойств (init, holdtime, wires, modbus_server...)";
                // ИСПРАВЛЕНО: Передаем true в качестве второго аргумента forceCreateAll!
                parseSchemaNode(menuTargetItem, singleSchema, QJsonValue(createDefaultData(singleSchema, true)));

                deviceTreeView->expand(menuTargetItem->index());
                modified = true; emit onChanged();
            });
        }
        if (!missingFixedParams.isEmpty() && !missingLinkOptions.isEmpty())
            addMissingMenu->addSeparator();
        // // Б) Восстановление стертых динамических каналов (chan1, modbus_client1...)
        // for (const MissingLinkOption &opt : missingLinkOptions) {
        //     addMissingMenu->addAction(opt.menuLabel, this, [this, menuTargetItem, opt, activeStruct]() {
        //         QJsonObject subStruct = opt.linkMeta.value("structure").toObject();
        //         qDebug() << "buildRestoreMenu" << "Б) Восстановление стертых динамических каналов (chan1, modbus_client1...)";
        //         // ИСПРАВЛЕНО: Передаем true в качестве второго аргумента forceCreateAll!
        //         QJsonObject defaultSubData = createDefaultData(subStruct, false);

        //         QString rawKey = opt.linkMeta.value("range").toBool() ? opt.insertKey.left(2) : opt.insertKey.left(4);
        //         if (!opt.linkMeta.contains("max")) {
        //             for (auto sIt = activeStruct.begin(); sIt != activeStruct.end(); ++sIt) {
        //                 if (sIt.value().toObject() == opt.linkMeta) { rawKey = sIt.key(); break; }
        //             }
        //         }
        //         applyVariablePostfix(defaultSubData, subStruct, opt.insertKey.mid(rawKey.length()));

        //         QJsonObject singleSchema; singleSchema.insert(opt.insertKey, opt.linkMeta);
        //         QJsonObject singleData; singleData.insert(opt.insertKey, defaultSubData);

        //         parseSchemaNode(menuTargetItem, singleSchema, QJsonValue(singleData));
        //         deviceTreeView->expand(menuTargetItem->index());
        //         modified = true; emit onChanged();
        //     });
        // }
        // Б) Восстановление стертых динамических каналов (chan1, holding...)
        // Б) Восстановление стертых динамических каналов (chan1, holding...)
        for (const MissingLinkOption &opt : missingLinkOptions) {
            addMissingMenu->addAction(opt.menuLabel, this, [this, menuTargetItem, opt, activeStruct]() {
                QJsonObject subStruct = opt.linkMeta.value("structure").toObject();
                qDebug() << "buildRestoreMenu" << "Б) Восстановление стертых динамических каналов";

                QJsonObject defaultSubData = createDefaultData(subStruct, false);

                // Извлекаем строку индекса
                QString calculatedIdxStr = opt.linkMeta.value("_calculated_idx").toString();

                QString rawKey = opt.linkMeta.value("range").toBool() ? opt.insertKey.left(2) : opt.insertKey.left(4);
                if (!opt.linkMeta.contains("max") && calculatedIdxStr.isEmpty()) {
                    for (auto sIt = activeStruct.begin(); sIt != activeStruct.end(); ++sIt) {
                        if (sIt.value().toObject() == opt.linkMeta) { rawKey = sIt.key(); break; }
                    }
                }

                QString postfix = !calculatedIdxStr.isEmpty() ? calculatedIdxStr : opt.insertKey.mid(rawKey.length());
                applyVariablePostfix(defaultSubData, subStruct, postfix);

                bool isKeynum = (opt.linkMeta.value("type").toString() == "keynum");

                // Формируем структуры для парсера дерева
                QJsonObject singleSchema;
                singleSchema.insert(opt.insertKey, opt.linkMeta);

                QJsonObject singleData;
                if (isKeynum) {
                    // Передаем вычисленный номер как строку, чтобы parseSchemaNode записал его в OriginalKeyRole
                    singleData.insert(opt.insertKey, postfix);
                } else {
                    singleData.insert(opt.insertKey, defaultSubData);
                }

                // Запоминаем количество строк ДО добавления, чтобы найти индекс новой строки
                int rowCountBefore = menuTargetItem->rowCount();

                parseSchemaNode(menuTargetItem, singleSchema, QJsonValue(singleData));

                // Находим созданные элементы
                QStandardItem *createdRowNameItem = nullptr;
                QStandardItem *createdRowValueItem = nullptr;

                // parseSchemaNode обычно добавляет строки в конец (appendRow)
                if (menuTargetItem->rowCount() > rowCountBefore) {
                    int newRowIdx = menuTargetItem->rowCount() - 1;
                    createdRowNameItem = menuTargetItem->child(newRowIdx, 0);
                    createdRowValueItem = menuTargetItem->child(newRowIdx, 1);
                }

                if (isKeynum && createdRowNameItem) {
                    // 1. Наполняем внутреннюю подструктуру регистра (var, var_out, order)
                    parseSchemaNode(createdRowNameItem, subStruct, QJsonValue(defaultSubData));

                    // 2. ЯВНО записываем рассчитанный номер в текстовое поле колонки Value
                    if (createdRowValueItem) {
                        createdRowValueItem->setText(postfix);
                        createdRowValueItem->setEditable(true); // Разрешаем редактирование номера

                        // 3. ФОКУС И АВТО-РЕДАКТИРОВАНИЕ: Переводим фокус ввода на ячейку Value
                        QModelIndex valueIndexModel = createdRowValueItem->index();
                        deviceTreeView->expand(menuTargetItem->index());
                        deviceTreeView->scrollTo(valueIndexModel);
                        deviceTreeView->setCurrentIndex(valueIndexModel);

                        // Запускаем режим редактирования ячейки с номером (курсор встанет туда автоматически)
                        deviceTreeView->edit(valueIndexModel);
                    }
                } else {
                    deviceTreeView->expand(menuTargetItem->index());
                }

                modified = true;
                emit onChanged();
            });
        }
    }
}
