#ifndef DEVICETREEDOCKWIDGET_H
#define DEVICETREEDOCKWIDGET_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QString>
#include "plcmanager.h"
#include "firmwareanalyzer.h"

class DeviceTreeDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeDockWidget(QWidget *parent = nullptr);
    void updateDevice(const plcManager::CommandContext &ctx,
                      const QMap<qsizetype,lbprocess::scaninfo>& scan);
    bool containsName(const QString& name);

    void editFirmwareRepository();
    void reloadFirmwareRepositoryFromSettings(bool showErrors = true);

signals:
    void requestConfig(const plcManager::CommandContext &ctx);
    void requestUpdate(const plcManager::CommandContext &ctx);
    // void requestFlash(const plcManager::CommandContext &ctx);
    void requestFlashAll(const plcManager::CommandContext &ctx);
    void requestFboot(const plcManager::CommandContext &ctx);

private slots:
    void showContextMenu(const QPoint& pos);
    void onTreeExpanded(const QModelIndex &index);

private:
    enum ItemRole {
        ModuleTypeRole = Qt::UserRole + 1,
        InstalledVersionRole,
        ModuleItemRole,
        VersionInfoRole
    };

    plcManager *lbplc = nullptr;
    QTreeView *treeView = nullptr;
    QStandardItemModel *treeModel = nullptr;
    firmwareAnalyzer *m_firmwareAnalyzer = nullptr;

    bool m_firmwareLoaded = false;
    bool m_repositoryPromptDeclined = false;
    QString m_repositoryRoot;

    QStandardItem *findPlcRoot(const QHostAddress& ipv6);
    QStandardItem *versionInfoItem(QStandardItem *moduleItem) const;

    bool ensureFirmwareRepository();
    bool chooseFirmwareRepository(bool allowClear = false);
    bool loadFirmwareRepository(const QString &repositoryRoot, bool showErrors);
    void clearFirmwareRepository();

    void updateFirmwareStatus(QStandardItem *moduleItem);
    void updateAllFirmwareStatuses();
    void clearAllFirmwareStatuses();

    // inline QString toBold(const QString &text);
};

#endif // DEVICETREEDOCKWIDGET_H
