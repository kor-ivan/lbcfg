#ifndef DEVICETREEWIDGET_H
#define DEVICETREEWIDGET_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <lbprocess.h>

class DeviceTreeWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeWidget(QWidget *parent = nullptr);
    void updateDevice(const QString& ipv6, const QString& name,
                      const QMap<qsizetype,lbprocess::scaninfo>& scan);

signals:
    void requestConfig(const QString& ipv6, const QString& name);

private slots:
    void showContextMenu(const QPoint& pos);

private:
    QTreeView *treeView = nullptr;
    QStandardItemModel *treeModel = nullptr;

    QStandardItem *findPlcRoot(const QString& ipv6);
};

#endif // DEVICETREEWIDGET_H
