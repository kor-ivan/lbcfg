#ifndef DEVICETREEDOCKWIDGET_H
#define DEVICETREEDOCKWIDGET_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include "plcmanager.h"

class DeviceTreeDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeDockWidget(QWidget *parent = nullptr);
    void updateDevice(const plcManager::CommandContext &ctx,
                      const QMap<qsizetype,lbprocess::scaninfo>& scan);
    bool containsName(const QString& name);

signals:
    void requestConfig(const plcManager::CommandContext &ctx);
    void requestUpdate(const plcManager::CommandContext &ctx);
    // void requestFlash(const plcManager::CommandContext &ctx);
    void requestFlashAll(const plcManager::CommandContext &ctx);
    void requestFboot(const plcManager::CommandContext &ctx);

private slots:
    void showContextMenu(const QPoint& pos);

private:
    plcManager *lbplc = nullptr;
    QTreeView *treeView = nullptr;
    QStandardItemModel *treeModel = nullptr;

    QStandardItem *findPlcRoot(const QHostAddress& ipv6);

    // inline QString toBold(const QString &text);
};

#endif // DEVICETREEDOCKWIDGET_H
