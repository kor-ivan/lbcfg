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
    void updateDevice(const plc::CommandContext &ctx,
                      const QMap<qsizetype,lbprocess::scaninfo>& scan);
    bool containsName(const QString& name);

    // void editFirmwareRepository();
    void reloadFirmwareRepositoryFromSettings();

signals:
    void requestConfig(const plc::CommandContext &ctx);
    void requestUpdate(const plc::CommandContext &ctx);
    // void requestFlash(const plc::CommandContext &ctx);
    void requestFlashAll(const plc::CommandContext &ctx, const QStringList &otaSlots = {});
    void requestFboot(const plc::CommandContext &ctx);

private slots:
    void showContextMenu(const QPoint& pos);
    void onTreeExpanded(const QModelIndex &index);

private:
    enum ItemRole {
        ModuleTypeRole = Qt::UserRole + 1,
        InstalledVersionRole,
        ModuleItemRole,
        VersionInfoRole,
        PlcUpToDateRole
    };

    plcManager *lbplc = nullptr;
    QTreeView *treeView = nullptr;
    QStandardItemModel *treeModel = nullptr;
    firmwareAnalyzer *m_firmwareAnalyzer = nullptr;

    bool m_firmwareLoaded = false;
    QString m_repositoryRoot;
    QStringList getMismatchedSlots(const QModelIndex &plcIndex) const;

    QStandardItem *findPlcRoot(const QHostAddress& ipv6);
    QStandardItem *versionInfoItem(QStandardItem *moduleItem) const;

    bool ensureFirmwareRepository();
    bool loadFirmwareRepository(const QString &repositoryRoot);

    bool updateFirmwareStatus(QStandardItem *moduleItem);
    void updateAllFirmwareStatuses();
    void clearAllFirmwareStatuses();

    const QColor SoftGreen = QColor(QStringLiteral("#C3E6CB"));
    void updatePlcRootStatus(QStandardItem *plcRoot, bool allModulesMatch);
};

#endif // DEVICETREEDOCKWIDGET_H
