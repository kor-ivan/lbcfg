#ifndef DEVICETREEDOCKWIDGET_H
#define DEVICETREEDOCKWIDGET_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <lbprocess.h>
#include <QMessageBox>

class DeviceTreeDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeDockWidget(QWidget *parent = nullptr);
    void updateDevice(const QString& ipv6, const QString& name,
                      const QMap<qsizetype,lbprocess::scaninfo>& scan);
    struct CommandContext {
        QString name;
        QString ipv6;
        int slot = -1;

        bool isSlot() const { return slot != -1; }
        QString displayName() const {
            return isSlot() ? QString("%1/slot %2").arg(name).arg(slot) : name;
        }
    };
    bool containsName(const QString& name);

signals:
    void requestConfig(const QString& ipv6, const QString& name);
    void requestUpdate(const QString& ipv6, const QString& name);
    void requestFlash(const QString& ipv6, const int& slot);

private slots:
    void showContextMenu(const QPoint& pos);

private:
    QTreeView *treeView = nullptr;
    QStandardItemModel *treeModel = nullptr;

    QStandardItem *findPlcRoot(const QString& ipv6);
    template <typename F>
    void lbc_executeCommand(const CommandContext &ctx,
                            const QStringList &args,
                            const QString &boxTitle,
                            F messageBuilder){
        LBclient *lbc = new LBclient(this, args);
        lbc->setTCPaddr(ctx.ipv6, 502);
        if (ctx.isSlot()) lbc->setSlot(ctx.slot);
        connect(lbc, &LBclient::ExecuteCompleted, this,
                [this, lbc, ctx, boxTitle, messageBuilder]
                (const QString& lbhost, const QStringList& result, const QString& message, const QModbusDevice::Error error){
                    QMessageBox::information(this, boxTitle, messageBuilder(result));
                    lbc->deleteLater();
                }
                );
        lbc->Execute();
    }

    inline QString toBold(const QString &text);
};

#endif // DEVICETREEDOCKWIDGET_H
