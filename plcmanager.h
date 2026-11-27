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

    discover::ModbusRtuSettings rtu;

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

using SerialPortList = QList<QSerialPortInfo>;
using SerialPortIterator = SerialPortList::const_iterator;

using RtuProfileList = QList<discover::ModbusRtuSettings>;
using RtuProfileIterator = RtuProfileList::const_iterator;

class plcManager : public QObject
{
    Q_OBJECT
    friend class WatchSession;

public:
    // explicit plcManager(QObject *parent = nullptr);
    // virtual ~plcManager();
    enum class ConnectionMode {
        TCPMode,
        RTUMode
    };
    Q_ENUM(ConnectionMode)
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
    void startDiscoverRTU(const RtuProfileList &profiles, const int startAddr, const int endAddr, const int timeOut);

    template <typename F>
    void lbc_executeCommand(const plc::CommandContext &ctx,
                            const QStringList &args,
                            const QString &boxTitle,
                            F messageBuilder){
        LBclient *lbc = new LBclient(this, args);
        if (!configureClientTransport(lbc, ctx)) return;
        // lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
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

    plc::CommandContext getctx(const QString &ipv6, const QString &name = {});
    plc::CommandContext getctx(const QHostAddress &host, const QString &name = {});
    const QMap<QString, discover::lbinfo>& getldmap() const;

    ConnectionMode connectionMode() const { return m_mode; }
    void setConnectionMode(ConnectionMode mode);
    void refreshSerialPorts(const QString &preferredPortName = QString());

    RtuProfileList rtuProfiles() const;
    RtuProfileIterator currentProfileIt() const;
    void setCurrentProfileIt(const RtuProfileIterator &newCurrentProfileIt);
    SerialPortList serialPorts() const;
    SerialPortIterator currentPortIt() const;
    void setCurrentPortIt(const SerialPortIterator &newCurrentPortIt);
    void reloadRtuProfiles();
    void stopDiscover();
    bool isDiscoverRunning() const { return discoverRunning; }

signals:
    void scanCompleted(const plc::CommandContext &ctx, const QMap<qsizetype, lbprocess::scaninfo> &scanData);
    void configReceived(const plc::CommandContext &ctx, const QString &yamlContent);
    void errorOccurred(const QString &message);
    void eventOccurred(const QString &message);
    void discoverStarting();
    void discoverCompleted(const QMap<QString, discover::lbinfo>& DiscoverMap);
    void discoverStopped();
    void firmwareStarted(const plc::CommandContext &ctx, const QString &message);
    void firmwareProgressChanged(int prc);
    void firmwareFinished();
    void logStarted();
    void logFinished();
    void confCompleted(const plc::CommandContext &ctx);
    void showMessage(const QString &title, const QString &message);
    void restartAllCompleted(const plc::CommandContext &ctx);
    void activeWatchChanged(const QStringList &keys);

    void connectionModeChanged(plcManager::ConnectionMode mode);
    void rtuProfilesUpdated(const RtuProfileList &profiles);
    void serialPortsUpdated(const SerialPortList &ports);
    void rtuDeviceDiscovered(const discover::lbinfo& info);



private:
    plcManager();
    ~plcManager() = default;
    plcManager(const plcManager&) = delete;
    plcManager& operator=(const plcManager&) = delete;
    static constexpr int port = 502;
    bool discoverRunning = false;
    QPointer<discover> activeDiscover;
    QPointer<LBclient> activeOtaClient;
    QPointer<lbprocess> prcActiveOtaClient;
    QPointer<LBclient> activeLogClient;
    void prcOtaSender(const plc::CommandContext &ctx, const QStringList &result, const QString &message, const QModbusDevice::Error error);

    QMap<QString, WatchSession*> activeWatchSessions;
    QMap<QString, discover::lbinfo> last_ldmap;
    bool SearchDiscoverRuning = false;
    QString getFastIfce(const discover::lbinfo &val);
    QString getIf(const QString &ipv6);

    ConnectionMode m_mode = ConnectionMode::TCPMode;
    bool configureClientTransport(LBclient *lbc, const plc::CommandContext &ctx);
    SerialPortList m_serialPorts;
    SerialPortIterator m_currentPortIt = m_serialPorts.cend();

    RtuProfileList m_rtuProfiles;
    RtuProfileIterator m_currentProfileIt = m_rtuProfiles.cend();
    discover::ModbusRtuSettings getRtu(const QString &ipv6);


};

#endif // PLCMANAGER_H
