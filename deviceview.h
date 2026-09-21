#ifndef DEVICEVIEW_H
#define DEVICEVIEW_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include "lbyaml.h"

class deviceView : public QWidget
{
    Q_OBJECT
public:
    enum DeviceRoles {
        SchemaMetaRole = Qt::UserRole,     // Сама JSON схема узла (для делегата)
        OriginalKeyRole = Qt::UserRole + 1, // Полный исходный ключ YAML (например, "holding0..1")
        SlotKeyRole = Qt::UserRole + 2,     // Чистый ключ слота (например, "slot-1")
        ModuleTypeRole = Qt::UserRole + 3   // Системный тип модуля (например, "bcbase")
    };

    explicit deviceView(QWidget *parent = nullptr);
    bool loadModulesSchema(const QString &jsonPath);
    void updateData(lbyaml *parser);

    QJsonObject getUpdateData();

    bool isModified() const;
    void resetModified();

    QStringList getAllProjectVariables() const;

signals:
    void onChanged();

private:
    QTreeView *deviceTreeView = nullptr;
    QStandardItemModel *deviceModel = nullptr;
    QJsonObject m_schemaRoot;
    bool m_schemaLoaded = false;

    // Рекурсивный обход данных и метаданных
    void parseSchemaNode(QStandardItem *parentNode, const QJsonObject &schemaNode, const QJsonValue &yamlData);

    // Поиск схемы модуля по значению "module" (например, "bcdi" -> схема для LB241DI16)
    QJsonObject findModuleSchema(const QString &moduleValue);

    // Распаковка сложных ключей-диапазонов (holding0..5, holding100+5)
    struct KeyRange { QString prefix; int start; int end; bool isValid; };
    KeyRange parseKeyRange(const QString &key);

    QJsonObject serializeNode(QStandardItem *parentNode);
    bool modified = false;
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    void showContextMenu(const QPoint &pos);
    QJsonObject createDefaultData(const QJsonObject &structureSchema, bool forceCreateAll = false);

    struct ContextMenuContext {
        bool isValidClick = false;
        bool isBlankSpace = false;
        bool allowDuplicateSlot = false; // Разрешить дублирование текущего модуля/слота

        QStandardItem *targetSectionItem = nullptr; // Элемент, по которому кликнули
        QStandardItem *parentContainer = nullptr;   // Физический родитель для операций вставки/удаления
        QJsonObject anyStruct;                      // Активная структура схемы для поиска стёртых полей
        QString varDescription;
        QJsonObject moduleObjMeta;       // Метаданные конкретного модуля из JSON-схемы

        // Декларативные флаги-команды для сборки UI
        bool showSlotManagement = false;  // Показать перемещение и удаление всего слота железа
        bool showRootBlockDelete = false; // Показать удаление обычного корневого блока (clock, forte)
        bool allowAddSequenceItem = false;// Разрешить добавление элемента в массив (forte->var)
        bool allowDeleteSequenceItem = false; // Разрешить удаление элемента из массива
        bool allowAddGlobalVariable = false;  // Разрешить создание глобальной переменной проекта
        bool allowDeleteVariableOrParam = false; // Разрешить удаление параметра/переменной/канала
        bool allowVariableExportToForte = false; // Разрешить экспорт переменной в массивы Forte
        bool allowRestoreSchemaParams = false;   // Разрешить подменю "Добавить отсутствующие параметры"

        // Вспомогательные маркеры для лямбд
        bool isAnyMode = false;
        bool isSequenceMode = false;
        QString currentItemText;
    };

    ContextMenuContext analyzeMenuContext(const QModelIndex &index);
    void reindexSlotsOfType(bool isBaseType);
    void applyVariablePostfix(QJsonObject &defaultSubData, const QJsonObject &subStruct, const QString &postfix);
    void buildRestoreMenu(QMenu *parentMenu, QStandardItem *menuTargetItem, const QJsonObject &activeStruct);

    void insertAndEditNewRow(QStandardItem *parentItem, const QString &description = "")
    {
        insertAndEditNewRow(parentItem, description, [](QStandardItem*) {});
    }

    void duplicateSlot(QStandardItem *sourceSlotItem, const QJsonObject &modObjMeta);
    QList<QStandardItem*> duplicateTreeViewNode(QStandardItem *sourceItem);
    // void postProcessVariablesPostfix(QStandardItem *item, const QString &oldPostfix, const QString &newPostfix);

    template <typename Callable>
    void insertAndEditNewRow(QStandardItem *parentItem,
                             const QString &description = "",
                             Callable midProcessing = nullptr)
    {
        if (!parentItem || !deviceTreeView) return;
        // Автоматическая генерация имени на основе родительского узла
        int newIdx = parentItem->rowCount() + 1;
        QString prefix = parentItem->text();
        QString defaultName = QString("%1_%2").arg(prefix).arg(newIdx);

        QStandardItem *newItemName = new QStandardItem(defaultName);
        newItemName->setEditable(true);

        QStandardItem *newItemValue = new QStandardItem("");
        newItemValue->setEditable(false);

        QStandardItem *newItemDesc = new QStandardItem(description);
        newItemDesc->setEditable(false);

        parentItem->insertRow(0, {newItemName, newItemValue, newItemDesc});

        midProcessing(newItemName);

        QModelIndex newIndexModel = newItemName->index();
        deviceTreeView->expand(parentItem->index());
        deviceTreeView->scrollTo(newIndexModel);
        deviceTreeView->setCurrentIndex(newIndexModel);
        deviceTreeView->edit(newIndexModel);

        modified = true;
        emit onChanged();

    }

};

#endif // DEVICEVIEW_H
