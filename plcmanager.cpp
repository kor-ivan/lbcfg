#include "plcmanager.h"
#include "logmanager.h"
#include "watchsession.h"
#include <QEventLoop>


plcManager::plcManager()
{}

// plcManager::~plcManager()
// {}

void plcManager::scanDevice(const CommandContext &ctx)
{
    debugApp() << "plcManager::Starting process for:"<<ctx.ipv6str()<<ctx.ipv6.scopeId()<<ctx.name;
    LBclient *lbc = new LBclient(this);
    lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    connect(lbc, &LBclient::lbDisconnect, this,
            [lbc](const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                debugPLC(lbhost)<<message<<"disconnect";
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
            [ctx, lbc, lbproc, this](const QMap<qsizetype, lbprocess::scaninfo>& scan){
                for (auto i = scan.begin(); i != scan.end(); ++i) {
                    debugPLC()<<i.key()<<i.value();
                }
                emit scanCompleted(ctx, scan);
                lbproc->deleteLater();
                lbc->deleteLater();
            }
            );
    lbproc->run(lbprocess::scan, {"sys.serial"});
}

void plcManager::requestConfig(const CommandContext &ctx)
{
    debugApp()<<"plcManager::getlbcfg: "<<ctx.ipv6str()<<ctx.ipv6.scopeId()<<ctx.name;
    LBclient *lbc = new LBclient(this, {"getconf"});
    lbc->setTCPaddr(ctx.ipv6str(), 502, ctx.ipv6.scopeId());
    connect(lbc, &LBclient::ExecuteCompletedJson, this,
            [lbc, this, ctx](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    QString yamlContent = lbyaml::getlbconf(Qjo, lbyaml::retainY);
                    debugApp()<<"# BEGIN YAML";
                    logPLC(ctx.name, LogCatcher::Debug, LogCatcher::wrapYes)<<yamlContent;
                    // lbyaml::printlbconf(Qjo);
                    debugApp()<<"# END YAML";
                    // Получаем YAML-текст один раз, чтобы использовать его для сравнения

                    emit configReceived(ctx, yamlContent);
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
    emit discoverStarting();
    discover *wgtdiscover = new discover(this);
    connect(wgtdiscover, &discover::discoverCompleted, this,
            [this, wgtdiscover] (const QMap<QString, discover::lbinfo>& DiscoverMap, const discover::discoverError error, const QString errorStr){
                discoverRunning = false;
                if (error != discover::NoError){
                    emit errorOccurred(errorStr);
                    return;
                }
                last_ldmap = DiscoverMap;
                emit discoverCompleted(DiscoverMap);
                wgtdiscover->deleteLater();
            }
            );
    discoverRunning = true;
    wgtdiscover -> execute();
}

bool plcManager::startFirmware(const CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage, const QString &lbkey)
{
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return false;
    }

    emit firmwareStarted(ctx, startMessage);
    activeOtaClient = new LBclient(this, {lbkey});
    // QPointer<LBclient> otaClient(activeOtaClient);
    activeOtaClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    activeOtaClient->setOtaFilename(filePath);
    if (ctx.slot != -1)
        activeOtaClient->setSlot(ctx.slot);

    connect(activeOtaClient, &LBclient::ExecuteCompleted, this, &plcManager::prcOtaSender);
    connect(activeOtaClient, &LBclient::lbDisconnect, this,
            [this](const QString &, const QString &message, const QModbusDevice::Error) {
                if (!message.isEmpty())
                    emit eventOccurred(message);
                emit firmwareFinished();
                if (activeOtaClient)
                    activeOtaClient->deleteLater();
            });

    activeOtaClient->Execute();
    return true;
}

void plcManager::stopFirmware()
{
    if (!activeOtaClient)
        return;

    QObject::disconnect(activeOtaClient, nullptr, this, nullptr);
    activeOtaClient->lbDisconnectDevice();

    if (prcActiveOtaClient) {
        prcActiveOtaClient->deleteLater();
    }
    if (activeOtaClient)
        activeOtaClient->deleteLater();
    emit firmwareFinished();
}

void plcManager::startConf(const CommandContext &ctx, const QString &yamlFilePath)
{
    debugApp()<<"plcManager::startConf for "<<ctx.name;
    LBclient *lbc = new LBclient(this, {"conf"});
    QString ifce = getIf(ctx.ipv6str());
    lbc->setlbHost(ctx.name, yamlFilePath, ifce);
    connect(lbc, &LBclient::ExecuteCompletedStr, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                if (lbstr!="OK")
                    emit errorOccurred(lbstr);
                else
                    emit eventOccurred(lbstr);
            });
    connect(lbc, &LBclient::lbDisconnect, this, [lbc, ctx, ifce, this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                plcManager::CommandContext m_ctx;
                m_ctx = ctx;
                m_ctx.ipv6.setScopeId(ifce);
                emit confCompleted(m_ctx);
                lbc->deleteLater();
            });
    lbc->Execute();
}

void plcManager::startFirmwareAll(const CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage)
{
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return;
    }
    activeOtaClient = new LBclient(this);
    activeOtaClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    prcActiveOtaClient = new lbprocess(this, activeOtaClient);
    prcActiveOtaClient->setOtaPath(filePath);

    emit firmwareStarted(ctx, startMessage);
    connect(prcActiveOtaClient, &lbprocess::outMessage, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                debugApp()<<lbstr<<message<<error;
                emit errorOccurred(lbstr);
            });
    connect(prcActiveOtaClient, &lbprocess::outOta, this, &plcManager::prcOtaSender);
    connect(activeOtaClient, &LBclient::lbDisconnect, this,
            [this]
            (const QString &, const QString &message, const QModbusDevice::Error){
                if (!message.isEmpty())
                    emit eventOccurred(message);
                emit firmwareFinished();

                if (prcActiveOtaClient)
                    prcActiveOtaClient->deleteLater();
                if (activeOtaClient)
                    activeOtaClient->deleteLater();
            });
    prcActiveOtaClient->run(lbprocess::autoota);
}

void plcManager::startRestartAll(const CommandContext &ctx)
{
    debugApp()<<"plcManager::startRestartAll for"<<ctx.ipv6<<ctx.ipv6.scopeId();
    LBclient *lbc = new LBclient (this);
    lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
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
    if (activeLogClient){
        debugApp() << "Log is already running, stop the current log one first";
        return;
    }
    debugApp()<<QString("plcManager::startLog for %1 if %3 slot %2")
                      .arg(ctx.ipv6str()).arg(ctx.slot).arg(ctx.ipv6.scopeId())
               << activeLogClient.get();

    activeLogClient = new LBclient (this, {"log", flag});
    if (ctx.slot!=-1)
        activeLogClient->setSlot(ctx.slot);
    activeLogClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    connect(activeLogClient, &LBclient::ExecuteCompletedStr, this, [this, ctx]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                if (error==QModbusDevice::NoError){
                    rawPLC(ctx)<<lbstr;
                }
                else
                    emit errorOccurred(lbstr);
            });
    connect(activeLogClient, &LBclient::lbDisconnect, this, [this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error){
                if (!message.isEmpty())
                    debugPLC()<<message;
                if (activeLogClient)
                    activeLogClient->deleteLater();
                emit logFinished();
            });
    emit logStarted();
    activeLogClient->Execute();
}

void plcManager::stopLog()
{
    // qDebug()<<"into stopLog"<<activeLogClient.get() << activeLogClient.isNull();
    if (!activeLogClient)
        return;
    if (activeLogClient)
        activeLogClient->deleteLater();
    emit logFinished();
}

WatchSession *plcManager::startWatch(const CommandContext &ctx, const QStringList &arg, QObject *p_watchDock)
{
    if (activeWatchSessions.contains(ctx.name)) {
        debugApp() << "WatchSession for key" << ctx.name << "already exists. Returning existing session.";
        return activeWatchSessions.value(ctx.name);
    }

    debugApp() << "Creating new WatchSession for key:" << ctx.name;
    WatchSession *session = new WatchSession(ctx, arg, p_watchDock);
    activeWatchSessions.insert(ctx.name, session);
    emit activeWatchChanged(activeWatchSessions.keys());
    connect(session, &WatchSession::watchErrorOccurred, this, &plcManager::errorOccurred);
    connect(session, &QObject::destroyed, this, [this, ctx]() {
        activeWatchSessions.remove(ctx.name);
        emit activeWatchChanged(activeWatchSessions.keys());
        debugApp() << "WatchSession removed from manager for key:" << ctx.name;
    });
    return session;
}

QStringList plcManager::activeWatchKeys() const
{
    return activeWatchSessions.keys();
}

QString plcManager::getFastIfce(const discover::lbinfo &val)
{
    auto minIt = std::min_element(val.delay.constBegin(), val.delay.constEnd());
    auto minIndex = std::distance(val.delay.constBegin(), minIt);
    if (SearchDiscoverRuning) SearchDiscoverRuning = false;
    QString ifce = QString::number(val.ifindex.at(minIndex));
    debugApp() << QString("preferred interface is %1 whith %2 ms").arg(ifce).arg(val.delay.at(minIndex));
    return ifce;
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

const QMap<QString, discover::lbinfo>& plcManager::getldmap() const
{
    return last_ldmap;
}

QString plcManager::getIf(const QString &ipv6)
{
    if (!last_ldmap.isEmpty()){
        if (last_ldmap.contains(ipv6)){
            //Ищем индекс интерфейса с минимальным временем отклика
            if (SearchDiscoverRuning) SearchDiscoverRuning = false;
            return getFastIfce(last_ldmap.value(ipv6));
        }
    }else if (!SearchDiscoverRuning){
        //Если discover ни разу не запускали или ничего не нашли
        startDiscover();
        debugApp() << "interface search runing...";
        SearchDiscoverRuning = true;
        QEventLoop loop;
        connect(this, &plcManager::discoverCompleted, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        return getIf(ipv6);
    }else{
        SearchDiscoverRuning = false;
        debugApp() << "The interface was not found";
    }
    return QString();
}

plcManager::CommandContext plcManager::getctx(const QString &ipv6, const QString &name, const QString &ifce)
{
    plcManager::CommandContext ctx;
    ctx.ipv6 = QHostAddress(ipv6);
    if (!ifce.isEmpty()) ctx.ipv6.setScopeId(ifce);
    if (!name.isEmpty()) ctx.name = name;
    return ctx;
}

plcManager::CommandContext plcManager::getctx(const QHostAddress &host, const QString &name)
{
    plcManager::CommandContext ctx;
    ctx.ipv6 = host;
    if (!name.isEmpty()) ctx.name = name;
    return ctx;
}
