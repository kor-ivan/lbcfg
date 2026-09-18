#ifndef PLCMANAGER_H
#define PLCMANAGER_H

#include <QObject>
#include <QPointer>
#include "lbprocess.h"
#include "discover.h"
#include "lbclient.h"

class WatchSession;

class plcManager : public QObject
{
    Q_OBJECT
public:
    // explicit plcManager(QObject *parent = nullptr);
    // virtual ~plcManager();
    static plcManager* instanse()
    {
        static plcManager inst;
        return &inst;
    }
    struct CommandContext {
        QString name;
        QHostAddress ipv6;
        int slot = -1;

        bool isSlot() const { return slot != -1; }
        QString displayName() const {
            return isSlot() ? QString("%1/slot %2").arg(name).arg(slot) : name;
        }
        QString ipv6str() const {
            return QHostAddress(ipv6.toIPv6Address()).toString();
        }
    };

    void scanDevice(const CommandContext &ctx);
    void requestConfig(const CommandContext &ctx);
    void startDiscover();
    bool startFirmware(const CommandContext &ctx, const QString &filePath,
                       const QString &checkMessage,
                       const QString &startMessage,
                       const QString &lbkey = "ota");
    void stopFirmware();
    void startConf(const CommandContext &ctx, const QString &yamlFilePath);
    void startFirmwareAll(const CommandContext &ctx, const QString &filePath,
                          const QString &checkMessage,
                          const QString &startMessage);
    void startRestartAll (const CommandContext &ctx);
    void startFbootDownload(const CommandContext &ctx, const QString &filePath);
    void startLog (const CommandContext &ctx, const QString &flag);
    void stopLog();

    template <typename F>
    void lbc_executeCommand(const CommandContext &ctx,
                            const QStringList &args,
                            const QString &boxTitle,
                            F messageBuilder){
        LBclient *lbc = new LBclient(this, args);
        lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
        if (ctx.isSlot()) lbc->setSlot(ctx.slot);
        connect(lbc, &LBclient::ExecuteCompleted, this,
                [this, lbc, ctx, boxTitle, messageBuilder]
                (const QString& lbhost, const QStringList& result, const QString& message, const QModbusDevice::Error error){
                    emit showMessage(boxTitle, messageBuilder(result));
                    lbc->deleteLater();
                }
                );
        connect(lbc, &LBclient::lbDisconnect, this, [this]
                (const QString &lbhost, const QString &message, const QModbusDevice::Error error){
                    if (!message.isEmpty())
                        emit eventOccurred(message);
                });
        lbc->Execute();
    }

    WatchSession* startWatch(const CommandContext &ctx, const QStringList &arg, QObject *p_watchDock = nullptr);
    QStringList activeWatchKeys() const;
    QString getFastIfce(const discover::lbinfo &val);
    QString getIf(const CommandContext &ctx);

signals:
    void scanCompleted(const CommandContext &ctx, const QMap<qsizetype, lbprocess::scaninfo> &scanData);
    void configReceived(const CommandContext &ctx, const QString &yamlContent);
    void errorOccurred(const QString &message);
    void eventOccurred(const QString &message);
    void discoverStarting();
    void discoverCompleted(const QMap<QString, discover::lbinfo>& DiscoverMap);
    void firmwareStarted(const CommandContext &ctx, const QString &message);
    void firmwareProgressChanged(int prc);
    // void firmwareStatusChanged(const QString &message);
    void firmwareFinished();
    void logStarted();
    void logFinished();
    void confCompleted(const CommandContext &ctx);
    void showMessage(const QString &title, const QString &message);
    void restartAllCompleted(const CommandContext &ctx);
    void activeWatchChanged(const QStringList &keys);

private:
    plcManager();
    ~plcManager() = default;
    plcManager(const plcManager&) = delete;
    plcManager& operator=(const plcManager&) = delete;
    static constexpr int port = 502;
    bool discoverRunning = false;
    QPointer<LBclient> activeOtaClient;
    QPointer<lbprocess> prcActiveOtaClient;
    QPointer<LBclient> activeLogClient;
    void prcOtaSender(const QString &lbhost, const QStringList &result, const QString &message, const QModbusDevice::Error error);

    QMap<QString, WatchSession*> activeWatchSessions;
    QMap<QString, discover::lbinfo> last_ldmap;
    bool SearchDiscoverRuning = false;
};

#endif // PLCMANAGER_H
