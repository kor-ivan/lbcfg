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

};

#endif // DEVICEVIEW_H
