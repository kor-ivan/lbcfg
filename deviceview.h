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
    QJsonObject m_schemaObject;

    void buildSlotNode(QStandardItem *parentItem, const QString &slotName, const QVariantMap &slotYamlData);
    void generateStructure(QStandardItem *parentNode, const QJsonObject &structSchema, const QVariantMap &currentValues);

};

#endif // DEVICEVIEW_H
