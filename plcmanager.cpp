#include "plcmanager.h"
#include "logmanager.h"
#include "watchsession.h"
#include "appsettings.h"
#include "discoverdockwidget.h"
#include <QEventLoop>


plcManager::plcManager()
{}

void plcManager::scanDevice(const plc::CommandContext &ctx)
{
    logApp() << QString(tr("Starting scan process for: %1; IF = %2; name = %3; rtu = %4")).arg(ctx.ipv6str(), ctx.ipv6.scopeId(), ctx.name, QString::number(ctx.rtu.addr));
    LBclient *lbc = new LBclient(this);
    if (!configureClientTransport(lbc, ctx)) return;
    // lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    connect(lbc, &LBclient::lbDisconnect, this,
            [lbc](const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                logApp() << message << tr("disconnect") << lbhost;
                lbc->deleteLater();
            }
            );
    lbprocess *lbproc = new lbprocess(this, lbc);
    lbproc->setNumOfVarRetries(AppSettings::getMaxConfigReadRetries());
    connect(lbproc, &lbprocess::outMessage, this,
            [ctx](const QString &lbstr, const QString &message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError)
                    logPLC(ctx.name)<<lbstr;
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

void plcManager::requestConfig(const plc::CommandContext &ctx)
{
    logApp()<<QString(tr("Starting request config process for: %1; IF = %2; name = %3")).arg(ctx.ipv6str(), ctx.ipv6.scopeId(), ctx.name);
    LBclient *lbc = new LBclient(this, {"getconf"});
    if (!configureClientTransport(lbc, ctx)) return;
    // lbc->setTCPaddr(ctx.ipv6str(), 502, ctx.ipv6.scopeId());
    connect(lbc, &LBclient::ExecuteCompletedJson, this,
            [lbc, this, ctx](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    QString yamlContent = lbyaml::getlbconf(Qjo,
                                                            lbyaml::retainY,
                                                            AppSettings::getMaxFlowLevel(),
                                                            static_cast<YAML::EmitterStyle::value>(AppSettings::getStyleYamlArray()));
                    logPLC(ctx.name, LogCatcher::Debug) << "# BEGIN YAML";
                    logPLC(ctx.name, LogCatcher::Debug, LogCatcher::wrapYes)<<yamlContent;
                    logPLC(ctx.name, LogCatcher::Debug) << "# END YAML";
                    logApp() << tr("Request config is completed for:") << lbhost;

                    plc::CommandContext updatedCtx = ctx;

                    if (m_mode == ConnectionMode::RTUMode && updatedCtx.name == "noname" && !Qjo.isEmpty()) {
                        auto jit = Qjo.find("hostname");
                        if (jit != Qjo.end() && jit->isString())
                            updatedCtx.name = jit->toString();
                        const QString key = updatedCtx.ipv6str();
                        if (last_ldmap.contains(key)) {
                            last_ldmap[key].name = updatedCtx.name;
                        }
                    }

                    emit configReceived(updatedCtx, yamlContent);
                }
                else{
                    logPLC(ctx.name) << message;
                    emit errorOccurred(message);
                }
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}

void plcManager::startDiscover()
{
    if (discoverRunning){
        logApp() << tr("Discover allready runing");
        return;
    }
    logApp() << tr("Discover runing");
    activeDiscover = new discover(this);
    connect(activeDiscover, &discover::discoverCompleted, this,
            [this] (const QMap<QString, discover::lbinfo>& DiscoverMap, const discover::discoverError error, const QString errorStr){
                discoverRunning = false;
                if (error != discover::NoError){
                    emit errorOccurred(errorStr);
                    return;
                }
                last_ldmap = DiscoverMap;
                emit discoverCompleted(DiscoverMap);
                activeDiscover->deleteLater();
            }
            );
    discoverRunning = true;
    emit discoverStarting();
    activeDiscover -> execute();
}

bool plcManager::startFirmware(const plc::CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage, const QString &lbkey)
{
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return false;
    }

    emit firmwareStarted(ctx, startMessage);
    activeOtaClient = new LBclient(this, {lbkey});
    activeOtaClient -> setMaxOtaTimeOutCount(AppSettings::getMaxFirmwareRetries());
    if (!configureClientTransport(activeOtaClient, ctx)) return false;
    // activeOtaClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    activeOtaClient->setOtaFilename(filePath);
    if (ctx.slot != -1)
        activeOtaClient->setSlot(ctx.slot);

    connect(activeOtaClient, &LBclient::ExecuteCompleted, this, [this, ctx]
            (const QString& lbhost, const QStringList& result, const QString& message, const QModbusDevice::Error error){
        prcOtaSender(ctx, result, message, error);
    });
    connect(activeOtaClient, &LBclient::lbDisconnect, this,
            [this, ctx](const QString& lbhost, const QString& message, const QModbusDevice::Error error) {
                if (!message.isEmpty()){
                    emit eventOccurred(message);
                    logPLC(ctx.name) << message << error;
                }
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

void plcManager::startConf(const plc::CommandContext &ctx, const QString &yamlFilePath)
{
    logApp() << tr("Starting conf process for:") << ctx.name;
    LBclient *lbc = new LBclient(this, {"conf"});
    QString ifce = getIf(ctx.ipv6str());
    lbc->setlbHost(ctx.name, yamlFilePath, ifce);
    connect(lbc, &LBclient::ExecuteCompletedStr, this, [this, ctx]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                if (lbstr!="OK")
                    emit errorOccurred(lbstr);
                else
                    emit eventOccurred(lbstr);
                logPLC(ctx.name) << lbstr;
            });
    connect(lbc, &LBclient::lbDisconnect, this, [lbc, ctx, ifce, this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                logApp() << message << tr("disconnect") << lbhost;
                plc::CommandContext m_ctx;
                m_ctx = ctx;
                m_ctx.ipv6.setScopeId(ifce);
                emit confCompleted(m_ctx);
                lbc->deleteLater();
            });
    lbc->Execute();
}

void plcManager::startFirmwareAll(const plc::CommandContext &ctx, const QString &filePath, const QString &checkMessage, const QString &startMessage, const QStringList &otaSlots)
{
    if (activeOtaClient) {
        emit eventOccurred(checkMessage);
        return;
    }
    logPLC() << QString(tr("The command to flash all modules has been launched %1")).
                arg(AppSettings::getFirmwareIgnoreCPU()?tr("with the option: ignore the processor module"):"");
    activeOtaClient = new LBclient(this);
    if (!configureClientTransport(activeOtaClient, ctx)) return;
    // activeOtaClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    activeOtaClient -> setMaxOtaTimeOutCount(AppSettings::getMaxFirmwareRetries());
    prcActiveOtaClient = new lbprocess(this, activeOtaClient,
                                       static_cast<lbprocess::Strategy>(AppSettings::getFirmwareStrtegy()));
    prcActiveOtaClient->setOtaPath(filePath);
    prcActiveOtaClient->setNumOfVarRetries(AppSettings::getMaxConfigReadRetries());
    if (!otaSlots.isEmpty())
        prcActiveOtaClient -> setPreOtaSlot(otaSlots);
    emit firmwareStarted(ctx, startMessage);
    connect(prcActiveOtaClient, &lbprocess::outMessage, this, [this, ctx]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
                logPLC(ctx.name) <<lbstr<<message<<error;
                emit errorOccurred(lbstr);
            });
    connect(prcActiveOtaClient, &lbprocess::outOta, this, [this, ctx]
            (const QString& lbhost, const QStringList& result, const QString& message, const QModbusDevice::Error error){
                prcOtaSender(ctx, result, message, error);
            });
    connect(activeOtaClient, &LBclient::lbDisconnect, this,
            [this, ctx]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                if (!message.isEmpty()){
                    emit eventOccurred(message);
                    logPLC(ctx.name) << message << error;
                }
                emit firmwareFinished();

                if (prcActiveOtaClient)
                    prcActiveOtaClient->deleteLater();
                if (activeOtaClient)
                    activeOtaClient->deleteLater();
            });
    prcActiveOtaClient->run(lbprocess::autoota);
}

void plcManager::startRestartAll(const plc::CommandContext &ctx)
{
    debugApp()<<"plcManager::startRestartAll for"<<ctx.ipv6<<ctx.ipv6.scopeId();
    LBclient *lbc = new LBclient (this);
    if (!configureClientTransport(lbc, ctx)) return;
    // lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    lbc -> setMaxOtaTimeOutCount(AppSettings::getMaxFirmwareRetries());
    lbprocess *prc = new lbprocess(this, lbc);
    prc->setNumOfVarRetries(AppSettings::getMaxConfigReadRetries());
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

void plcManager::startLog(const plc::CommandContext &ctx, const QString &flag, const QString &fileName)
{
    if (activeLogClient){
        logApp() << tr("Log is already running, stop the current log one first");
        return;
    }
    logApp()<<QString(tr("StartLog for %1 if %3 slot %2"))
                      .arg(ctx.ipv6str()).arg(ctx.slot).arg(ctx.ipv6.scopeId())
               << activeLogClient.get();

    activeLogClient = new LBclient (this, {"log", flag});
    if (ctx.slot!=-1)
        activeLogClient->setSlot(ctx.slot);
    if (!configureClientTransport(activeLogClient, ctx)) return;
    // activeLogClient->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());

    QPointer<QFile> logFile;

    if (!fileName.isEmpty()) {
        logFile = new QFile(fileName, activeLogClient);
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            debugApp() << tr("Failed to open log file for writing:") << fileName;
            if (activeLogClient)
                activeLogClient->deleteLater();
            return;
        } else {
            QTextStream stream(logFile);
            stream << QString("--- Log started at %1 ---\n")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
            logApp() << tr("Log started:") << fileName;
        }
    }

    connect(activeLogClient, &LBclient::ExecuteCompletedStr, this,
            [this, ctx, fileName, logFile, lineCount = 0]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error) mutable {
                if (error==QModbusDevice::NoError){
                    if (fileName.isEmpty())
                        rawPLC(ctx)<<lbstr;
                    else{
                        QTextStream stream(logFile);
                        stream << '\n' << lbstr;
                        lineCount++;
                        if (lineCount % 100 == 0)
                            logApp() << QString(tr("[%1] Write line to file: %2")).arg(ctx.ipv6str()).arg(lineCount);
                    }
                }
                else
                    emit errorOccurred(lbstr);
            });
    connect(activeLogClient, &LBclient::lbDisconnect, this, [this, logFile, fileName]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error){
                if (!message.isEmpty())
                    debugPLC()<<message;
                if (logFile && logFile->isOpen()){
                    QTextStream stream(logFile);
                    stream << QString("\n--- Log finished at %1 ---")
                                  .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
                    logFile->close();
                    logApp() << tr("Log finished:") << fileName;
                }
                if (activeLogClient)
                    activeLogClient->deleteLater();
                emit logFinished();
            });
    emit logStarted();
    activeLogClient->Execute();
}

void plcManager::stopLog()
{
    if (!activeLogClient)
        return;
    if (activeLogClient)
        activeLogClient->deleteLater();
    emit logFinished();
}

void plcManager::startDiscoverRTU(const RtuProfileList &profiles, const int startAddr, const int endAddr, const int timeOut)
{
    if (discoverRunning) {
        logApp() << tr("Discover already running");
        return;
    }

    if (m_currentPortIt == m_serialPorts.cend()) {
        logApp() << (tr("RTU режим: нет доступных COM-портов или порт не выбран"));
        return;
    }

    const QString portName = m_currentPortIt->portName();
    RtuProfileList rtuList = profiles.isEmpty()
                                 ? (m_rtuProfiles.isEmpty() ? AppSettings::getRtuProfiles() : m_rtuProfiles)
                                 : profiles;
    logApp() << QString(tr("RTU Discover on %1 (addrs %2..%3, timeout %4 ms)"))
                    .arg(portName).arg(startAddr).arg(endAddr).arg(timeOut);

    activeDiscover = new discover(this);
    connect(activeDiscover, &discover::SendingRtuResponse, this,
            [this](const discover::ModbusRtuSettings &rtu) {
                logApp() << QString(tr("RTU опрос: %1, адрес %2 (%3)"))
                                       .arg(rtu.portName).arg(rtu.addr)
                        .arg(DiscoverDockWidget::formatRtuProfile(rtu));
            });
    connect(activeDiscover, &discover::discoverCompleted, this,
            [this](const QMap<QString, discover::lbinfo>& DiscoverMap,
                                const discover::discoverError error,
                                const QString errorStr) {
                discoverRunning = false;
                if (error != discover::NoError) {
                    emit errorOccurred(errorStr);
                    activeDiscover->deleteLater();
                    return;
                }
                last_ldmap = DiscoverMap;
                emit discoverCompleted(DiscoverMap);
                activeDiscover->deleteLater();
            });
    connect(activeDiscover, &discover::rtuResponseReceived, this,
            [this](const discover::lbinfo &info) {
                const QString key = lbyaml::MacToIPv6(info.mac);
                last_ldmap.insert(key, info);
                emit rtuDeviceDiscovered(info);
            });

    discoverRunning = true;
    emit discoverStarting();
    last_ldmap.clear();
    activeDiscover->executeRtu(rtuList, startAddr, endAddr, timeOut);
}

WatchSession *plcManager::startWatch(const plc::CommandContext &ctx, const QStringList &arg, QObject *p_watchDock)
{
    if (activeWatchSessions.contains(ctx.name)) {
        logApp() << tr("WatchSession for key") << ctx.name << tr("already exists. Returning existing session");
        return activeWatchSessions.value(ctx.name);
    }

    logApp() << tr("Creating new WatchSession for key:") << ctx.name;
    WatchSession *session = new WatchSession(ctx, arg, p_watchDock);
    activeWatchSessions.insert(ctx.name, session);
    emit activeWatchChanged(activeWatchSessions.keys());
    connect(session, &WatchSession::watchErrorOccurred, this, &plcManager::errorOccurred);
    connect(session, &QObject::destroyed, this, [this, ctx]() {
        activeWatchSessions.remove(ctx.name);
        emit activeWatchChanged(activeWatchSessions.keys());
        debugApp() << tr("WatchSession removed from manager for key:") << ctx.name;
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
    QString ifce = val.ifindex.at(minIndex);
    debugApp() << QString(tr("preferred interface is %1 whith %2 ms")).arg(ifce).arg(val.delay.at(minIndex));
    return ifce;
}

void plcManager::prcOtaSender(const plc::CommandContext &ctx, const QStringList &result, const QString &message, const QModbusDevice::Error error)
{
    if(error==QModbusDevice::NoError){
        int prc = (int)result.value(1, "").toFloat();
        emit firmwareProgressChanged(prc);
    }
    if (!message.isEmpty()){
        logPLC(ctx.name) << message;
        emit errorOccurred(message);
    }
}

const QMap<QString, discover::lbinfo>& plcManager::getldmap() const
{
    return last_ldmap;
}

void plcManager::setConnectionMode(ConnectionMode mode)
{
    if (m_mode == mode)
        return;

    if (mode == ConnectionMode::RTUMode) {
        refreshSerialPorts();
        reloadRtuProfiles();
    }

    m_mode = mode;
    emit connectionModeChanged(m_mode);
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
        debugApp() << tr("interface search runing...");
        SearchDiscoverRuning = true;
        QEventLoop loop;
        connect(this, &plcManager::discoverCompleted, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        return getIf(ipv6);
    }else{
        SearchDiscoverRuning = false;
        debugApp() << tr("The interface was not found");
    }
    return QString();
}

bool plcManager::configureClientTransport(LBclient *lbc, const plc::CommandContext &ctx)
{
    if (m_mode == ConnectionMode::RTUMode) {
        discover::ModbusRtuSettings rtu = ctx.rtu;
        if ((rtu.portName.isEmpty() || rtu.addr <= 0) && last_ldmap.contains(ctx.ipv6str())) {
            rtu = last_ldmap.value(ctx.ipv6str()).rtuSettings;
        }
        if (rtu.portName.isEmpty() || rtu.addr <= 0) {
            logApp() << (tr("RTU: не задан COM-порт или адрес устройства"));
            lbc->deleteLater();
            return false;
        }
        auto it = std::find_if(m_rtuProfiles.cbegin(), m_rtuProfiles.cend(),
                               [&ctx](const discover::ModbusRtuSettings &cfg) {
                                   return cfg.baudRate == ctx.rtu.baudRate &&
                                          cfg.dataBits == ctx.rtu.dataBits &&
                                          cfg.parity   == ctx.rtu.parity &&
                                          cfg.stopBits == ctx.rtu.stopBits;
                               });

        const QString requestedProfile = DiscoverDockWidget::formatRtuProfile(ctx.rtu);

        if (it != m_rtuProfiles.cend()) {
            setCurrentProfileIt(it);
            logApp() << tr("Активный профиль RTU изменён на %1.").arg(DiscoverDockWidget::formatRtuProfile(*it));
        } else {
            lbc->deleteLater();
            logApp() << tr("Профиль RTU (%1) не найден в списке. Добавьте профиль в настройки. Операция прервана.").arg(requestedProfile);
            return false;
        }
        lbc->setSerialAddr(rtu.portName, rtu.addr, ctx.rtu.baudRate, ctx.rtu.dataBits, ctx.rtu.parity, ctx.rtu.stopBits);
        return true;
    }

    lbc->setTCPaddr(ctx.ipv6str(), port, ctx.ipv6.scopeId());
    return true;
}

void plcManager::setCurrentPortIt(const SerialPortIterator &newCurrentPortIt)
{
    m_currentPortIt = newCurrentPortIt;
}

SerialPortIterator plcManager::currentPortIt() const
{
    return m_currentPortIt;
}

SerialPortList plcManager::serialPorts() const
{
    return m_serialPorts;
}

void plcManager::setCurrentProfileIt(const RtuProfileIterator &newCurrentProfileIt)
{
    m_currentProfileIt = newCurrentProfileIt;
}

RtuProfileIterator plcManager::currentProfileIt() const
{
    return m_currentProfileIt;
}

RtuProfileList plcManager::rtuProfiles() const
{
    return m_rtuProfiles;
}

void plcManager::reloadRtuProfiles()
{
    int prevBaud = -1, prevData = -1, prevParity = -1, prevStop = -1;
    const bool hadProfile = (m_currentProfileIt != m_rtuProfiles.cend());
    if (hadProfile) {
        prevBaud   = m_currentProfileIt->baudRate;
        prevData   = m_currentProfileIt->dataBits;
        prevParity = m_currentProfileIt->parity;
        prevStop   = m_currentProfileIt->stopBits;
    }

    m_rtuProfiles = AppSettings::getRtuProfiles();

    if (m_rtuProfiles.isEmpty()) {
        m_currentProfileIt = m_rtuProfiles.cend();
    } else if (hadProfile) {
        auto it = std::find_if(m_rtuProfiles.cbegin(), m_rtuProfiles.cend(),
                               [=](const discover::ModbusRtuSettings &cfg) {
                                   return cfg.baudRate == prevBaud &&
                                          cfg.dataBits == prevData &&
                                          cfg.parity   == prevParity &&
                                          cfg.stopBits == prevStop;
                               });
        m_currentProfileIt = (it != m_rtuProfiles.cend()) ? it : m_rtuProfiles.cbegin();
    } else {
        m_currentProfileIt = m_rtuProfiles.cbegin();
    }

    emit rtuProfilesUpdated(m_rtuProfiles);
    // emit currentRtuProfileChanged(m_currentProfileIt);
}

void plcManager::stopDiscover()
{
    if (!discoverRunning)
        return;

    discoverRunning = false;

    if (activeDiscover) {
        QObject::disconnect(activeDiscover, nullptr, this, nullptr);
        activeDiscover->deleteLater();
        activeDiscover.clear();
    }

    logApp() << tr("RTU Discover stopped by user");
    emit discoverStopped();
}

void plcManager::refreshSerialPorts(const QString &preferredPortName)
{
    QString targetName = preferredPortName;
    if (targetName.isEmpty() && m_currentPortIt != m_serialPorts.cend()) {
        targetName = m_currentPortIt->portName();
    }

    m_serialPorts = discover::getlbSerialDiscover();

    if (m_serialPorts.isEmpty()) {
        m_currentPortIt = m_serialPorts.cend();
    } else if (!targetName.isEmpty()) {
        auto it = std::find_if(m_serialPorts.cbegin(), m_serialPorts.cend(),
                               [&targetName](const QSerialPortInfo &info) {
                                   return info.portName() == targetName;
                               });
        m_currentPortIt = (it != m_serialPorts.cend()) ? it : m_serialPorts.cbegin();
    } else {
        m_currentPortIt = m_serialPorts.cbegin();
    }

    emit serialPortsUpdated(m_serialPorts);
    // emit currentSerialPortChanged(m_currentPortIt);
}

plc::CommandContext plcManager::getctx(const QString &ipv6, const QString &name)
{
    plc::CommandContext ctx;
    ctx.ipv6 = QHostAddress(ipv6);
    ctx.ipv6.setScopeId(getIf(ctx.ipv6str()));
    if (!name.isEmpty()) ctx.name = name;
    return ctx;
}

plc::CommandContext plcManager::getctx(const QHostAddress &host, const QString &name)
{
    plc::CommandContext ctx;
    ctx.ipv6 = host;
    if (!name.isEmpty()) ctx.name = name;
    return ctx;
}
