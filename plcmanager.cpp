#include "plcmanager.h"
#include "logmanager.h"


plcManager::plcManager(QObject *parent)
    : QObject{parent}
{}

plcManager::~plcManager()
{}

void plcManager::scanDevice(const QString &ipv6, const QString &name)
{
    debugApp() << "plcManager::Starting process for:"<<ipv6<<" "<<name;
    LBclient *lbc = new LBclient(this);
    lbc->setTCPaddr(ipv6, port);
    connect(lbc, &LBclient::lbDisconnect, this,
            [lbc](const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                debugPLC()<<message<<"disconnect";
                lbc->deleteLater();
            }
            );
    lbprocess *lbproc = new lbprocess(this, lbc);
    connect(lbproc, &lbprocess::outMessage, this,
            [](const QString &lbstr, const QString &message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError)
                    debugPLC()<<lbstr;
                else
                    debugPLC()<<message;
            }
            );
    connect(lbproc, &lbprocess::scanCompleted, this,
            [ipv6, name, lbc, lbproc, this](const QMap<qsizetype, lbprocess::scaninfo>& scan){
                for (auto i = scan.begin(); i != scan.end(); ++i) {
                    debugPLC()<<i.key()<<i.value();
                }
                emit scanCompleted(ipv6, name, scan);
                lbproc->deleteLater();
                lbc->deleteLater();
            }
            );
    lbproc->run(lbprocess::scan, {"sys.serial"});
}

void plcManager::requestConfig(const QString &ipv6, const QString &name)
{
    debugApp()<<"plcManager::getlbcfg: "<<ipv6<<name;
    LBclient *lbc = new LBclient(this, {"getconf"});
    lbc->setTCPaddr(ipv6, 502);
    connect(lbc, &LBclient::ExecuteCompletedJson, this,
            [lbc, this, name, ipv6](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    QString yamlContent = lbyaml::getlbconf(Qjo);
                    debugApp()<<"# BEGIN YAML";
                    logPLC(LogCatcher::Debug, LogCatcher::wrapYes)<<yamlContent;
                    // lbyaml::printlbconf(Qjo);
                    debugApp()<<"# END YAML";
                    // Получаем YAML-текст один раз, чтобы использовать его для сравнения

                    emit configReceived(ipv6, name, yamlContent);
                }
                else{
                    debugPLC()<<message;
                    emit errorOccurred(message);
                }
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}

void plcManager::startDiscover()
{
    debugApp()<<"startDiscover "<<discoverRunning;
    if (discoverRunning)
        return;
    discover *wgtdiscover = new discover(this);
    connect(wgtdiscover, &discover::discoverCompleted, this,
            [this, wgtdiscover] (const QMap<QString, discover::lbinfo>& DiscoverMap, const discover::discoverError error, const QString errorStr){
                discoverRunning = false;
                if (error != discover::NoError){
                    emit errorOccurred(errorStr);
                    return;
                }
                emit discoverCompleted(DiscoverMap);
                wgtdiscover->deleteLater();
            }
            );
    discoverRunning = true;
    wgtdiscover -> execute();
}

void plcManager::startFirmware(const CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage, const QString &lbkey)
{
    debugApp() << "PLCManager: startFirmware slot=" << ctx.slot;
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return;
    }
    emit firmwareStarted(ctx, startMessage);
    activeOtaClient = new LBclient(this, {lbkey});
    activeOtaClient->setTCPaddr(ctx.ipv6, port);
    activeOtaClient->setOtaFilename(filePath);
    if (ctx.slot!=-1)
        activeOtaClient->setSlot(ctx.slot);
    connect(activeOtaClient, &LBclient::ExecuteCompleted, this, &plcManager::prcOtaSender);
    connect(activeOtaClient, &LBclient::lbDisconnect, this, [this]
            (const QString &lbhost, const QString &message, const QModbusDevice::Error error){
                emit eventOccurred(message);
                emit firmwareFinished();
                activeOtaClient->deleteLater();
                activeOtaClient = nullptr;
            });
    activeOtaClient->Execute();
}

void plcManager::stopFirmware()
{
    if (!activeOtaClient) return;
    activeOtaClient->disconnect();
    emit firmwareFinished();
    if (prcActiveOtaClient){
        prcActiveOtaClient->deleteLater();
        prcActiveOtaClient = nullptr;
    }else
        activeOtaClient->deleteLater();
    activeOtaClient = nullptr;

}

void plcManager::startConf(const QString &name, const QString &yamlFilePath)
{
    debugApp()<<"plcManager::startConf for "<<name;
    LBclient *lbc = new LBclient(this, {"conf"});
    lbc->setlbHost(name, yamlFilePath);
    connect(lbc, &LBclient::ExecuteCompletedStr, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                if (lbstr!="OK")
                    emit errorOccurred(lbstr);
                else
                    emit eventOccurred(lbstr);
            });
    connect(lbc, &LBclient::lbDisconnect, this, [lbc, name, this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                emit confCompleted(lbhost, name);
                lbc->deleteLater();
            });
    lbc->Execute();
}

void plcManager::startFirmwareAll(const CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage)
{
    debugApp() << "plcManager::startFirmwareAll" << ctx.name;
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return;
    }
    activeOtaClient = new LBclient(this);
    activeOtaClient->setTCPaddr(ctx.ipv6, port);
    prcActiveOtaClient = new lbprocess(this, activeOtaClient);
    prcActiveOtaClient->setOtaPath(filePath);
    emit firmwareStarted(ctx, startMessage);
    connect(prcActiveOtaClient, &lbprocess::outMessage, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                // qDebug()<<lbstr<<message<<error;
                emit errorOccurred(lbstr);
            });
    connect(prcActiveOtaClient, &lbprocess::outOta, this, &plcManager::prcOtaSender);
    connect(activeOtaClient, &LBclient::lbDisconnect, this, [this]
            (const QString &lbhost, const QString &message, const QModbusDevice::Error error){
                // qDebug()<<lbhost<<message<<error;
                if (!message.isEmpty())
                    emit eventOccurred(message);
                emit firmwareFinished();
                prcActiveOtaClient->deleteLater();
                activeOtaClient = nullptr;
                prcActiveOtaClient = nullptr;
            });
    prcActiveOtaClient->run(lbprocess::autoota);
}

void plcManager::startRestartAll(const CommandContext &ctx)
{
    debugApp()<<"plcManager::startRestartAll for"<<ctx.ipv6;
    LBclient *lbc = new LBclient (this);
    lbc->setTCPaddr(ctx.ipv6, port);
    lbprocess *prc = new lbprocess(this, lbc);
    connect(prc, &lbprocess::outMessage, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                emit eventOccurred(lbstr);
            });
    connect(lbc, &LBclient::lbDisconnect, this, [this, prc, ctx]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                debugPLC()<<"plcManager::startRestartAll disconnect"<<message;
                emit restartAllCompleted(ctx);
                prc->deleteLater();
            });
    prc->run(lbprocess::restartall);
}

void plcManager::startLog(const CommandContext &ctx, const QString &flag)
{
    debugApp()<<QString("plcManager::startLog for %1 slot %2")
                      .arg(ctx.ipv6).arg(ctx.slot)<<activeLogClient.get();
    if (activeLogClient)
        return;
    activeLogClient = new LBclient (this, {"log", flag});
    if (ctx.slot!=-1)
        activeLogClient->setSlot(ctx.slot);
    activeLogClient->setTCPaddr(ctx.ipv6, port);
    connect(activeLogClient, &LBclient::ExecuteCompletedStr, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                if (error==QModbusDevice::NoError){
                    rawPLC()<<lbstr;
                    qDebug()<<lbstr;
                }
                else
                    emit errorOccurred(lbstr);
            });
    connect(activeLogClient, &LBclient::lbDisconnect, this, [this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                if (!message.isEmpty())
                    debugPLC()<<message;
                activeLogClient->deleteLater();
                debugApp()<<"disconnect LogClient: "<<lbhost;
                emit logFinished();
            });
    activeLogClient->Execute();
    emit logStarted();
}

void plcManager::stopLog()
{
    debugApp()<<"into stopLog"<<activeLogClient.get();
    if (!activeLogClient) return;
    activeLogClient->deleteLater();
    emit logFinished();
}

void plcManager::prcOtaSender(const QString &lbhost, const QStringList &result, const QString &message, const QModbusDevice::Error error)
{
    if(error==QModbusDevice::NoError){
        int prc = (int)result.value(1, "").toFloat();
        emit firmwareProgressChanged(prc);
    }
    if (!message.isEmpty())
        emit errorOccurred(message);
}
