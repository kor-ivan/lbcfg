#ifndef PLCMANAGER_H
#define PLCMANAGER_H

#include <QObject>
#include <QPointer>
#include "lbprocess.h"
#include "discover.h"
#include "lbclient.h"

class WatchSession;

namespace plc {
struct CommandContext {
    QString name;
    QHostAddress ipv6;
    int slot = -1;

    bool isSlot() const { return slot != -1; }
    QString displayName() const {
        return isSlot() ? QString("%1/slot %2").arg(name).arg(slot) : name;
    }
    QString ipv6str() const {
        if (ipv6.protocol() == QAbstractSocket::IPv6Protocol)
            return QHostAddress(ipv6.toIPv6Address()).toString();
        else if (ipv6.protocol() == QAbstractSocket::IPv4Protocol)
            return ipv6.toString();
        return QString();
    }
};
}

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

    void scanDevice(const plc::CommandContext &ctx);
    void requestConfig(const plc::CommandContext &ctx);
    void startDiscover();
    bool startFirmware(const plc::CommandContext &ctx, const QString &filePath,
                       const QString &checkMessage,
                       const QString &startMessage,
                       const QString &lbkey = "ota");
    void stopFirmware();
    void startConf(const plc::CommandContext &ctx, const QString &yamlFilePath);
    void startFirmwareAll(const plc::CommandContext &ctx, const QString &filePath,
                          const QString &checkMessage,
                          const QString &startMessage,
                          const QStringList &otaSlots);
    void startRestartAll (const plc::CommandContext &ctx);
    void startFbootDownload(const plc::CommandContext &ctx, const QString &filePath);
    void startLog (const plc::CommandContext &ctx, const QString &flag, const QString &fileName = {});
    void stopLog();

    template <typename F>
    void lbc_executeCommand(const plc::CommandContext &ctx,
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

    WatchSession* startWatch(const plc::CommandContext &ctx, const QStringList &arg, QObject *p_watchDock = nullptr);
    QStringList activeWatchKeys() const;

    QString getIf(const QString &ipv6);
    plc::CommandContext getctx(const QString &ipv6, const QString &name = {}, const QString &ifce = {});
    plc::CommandContext getctx(const QHostAddress &host, const QString &name = {});
    const QMap<QString, discover::lbinfo>& getldmap() const;

signals:
    void scanCompleted(const plc::CommandContext &ctx, const QMap<qsizetype, lbprocess::scaninfo> &scanData);
    void configReceived(const plc::CommandContext &ctx, const QString &yamlContent);
    void errorOccurred(const QString &message);
    void eventOccurred(const QString &message);
    void discoverStarting();
    void discoverCompleted(const QMap<QString, discover::lbinfo>& DiscoverMap);
    void firmwareStarted(const plc::CommandContext &ctx, const QString &message);
    void firmwareProgressChanged(int prc);
    void firmwareFinished();
    void logStarted();
    void logFinished();
    void confCompleted(const plc::CommandContext &ctx);
    void showMessage(const QString &title, const QString &message);
    void restartAllCompleted(const plc::CommandContext &ctx);
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
    QString getFastIfce(const discover::lbinfo &val);
};

#endif // PLCMANAGER_H
