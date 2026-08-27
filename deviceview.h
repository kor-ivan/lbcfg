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
    explicit deviceView(QWidget *parent = nullptr);
    bool loadModulesSchema(const QString &jsonPath);
    void updateData(lbyaml *parser);

signals:

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

};

#endif // DEVICEVIEW_H
