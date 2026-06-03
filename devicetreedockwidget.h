#ifndef DEVICETREEDOCKWIDGET_H
#define DEVICETREEDOCKWIDGET_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <lbprocess.h>

class DeviceTreeDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeDockWidget(QWidget *parent = nullptr);
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

#endif // DEVICETREEDOCKWIDGET_H
