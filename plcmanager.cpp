#include "plcmanager.h"
#include "lbclient.h"

plcManager::plcManager(QObject *parent)
    : QObject{parent}
{}

plcManager::~plcManager()
{}

void plcManager::scanDevice(const QString &ipv6, const QString &name)
{
    qDebug() << "plcManager::Starting process for:"<<ipv6<<" "<<name;
    LBclient *lbc = new LBclient(this);
    lbc->setTCPaddr(ipv6, port);
    connect(lbc, &LBclient::lbDisconnect, this,
            [lbc](const QString& lbhost, const QString& message, const QModbusDevice::Error error){
                qDebug()<<message<<"disconnect";
                lbc->deleteLater();
            }
            );
    lbprocess *lbproc = new lbprocess(this, lbc);
    connect(lbproc, &lbprocess::outMessage, this,
            [](const QString &lbstr, const QString &message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError)
                    qDebug().noquote()<<lbstr;
                else
                    qDebug().noquote()<<message;
            }
            );
    connect(lbproc, &lbprocess::scanCompleted, this,
            [ipv6, name, lbc, lbproc, this](const QMap<qsizetype, lbprocess::scaninfo>& scan){
                for (auto i = scan.begin(); i != scan.end(); ++i) {
                    qDebug()<<i.key()<<i.value();
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
    qDebug()<<"plcManager::getlbcfg: "<<ipv6<<name;
    LBclient *lbc = new LBclient(this, {"getconf"});
    lbc->setTCPaddr(ipv6, 502);
    connect(lbc, &LBclient::ExecuteCompletedJson, this,
            [lbc, this, name, ipv6](const QString& lbhost, const QJsonObject& Qjo, const QString& message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    qDebug()<<"# BEGIN YAML";
                    lbyaml::printlbconf(Qjo);
                    qDebug()<<"# END YAML";
                    // Получаем YAML-текст один раз, чтобы использовать его для сравнения
                    QString yamlContent = lbyaml::getlbconf(Qjo);
                    emit configReceived(ipv6, name, yamlContent);
                }
                else{
                    qDebug().noquote()<<message;
                    emit errorOccurred(message);
                }
                lbc->deleteLater();
            }
            );
    lbc->Execute();
}

void plcManager::startDiscover()
{
    qDebug()<<"startDiscover "<<discoverRunning;
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

void plcManager::startFirmware(const QString &ipv6, const QString &filePath, int slot)
{
    qDebug() << "PLCManager: startFirmware slot=" << slot;
    if (activeOtaClient) {
        emit eventOccurred("Прошивка уже выполняется на другом устройстве");
        return;
    }
    emit firmwareStarted(ipv6);
    activeOtaClient = new LBclient(this, {"ota"});
    activeOtaClient->setTCPaddr(ipv6, 502);
    activeOtaClient->setOtaFilename(filePath);
    if (slot!=-1)
        activeOtaClient->setSlot(slot);
    connect(activeOtaClient, &LBclient::ExecuteCompleted, this, [this]
            (const QString &lbhost, const QStringList &result, const QString &message, const QModbusDevice::Error error){
                if(error==QModbusDevice::NoError){
                    int prc = (int)result.at(1).toFloat();
                    emit firmwareProgressChanged(prc);
                }else
                    emit errorOccurred(message);

            });
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
    activeOtaClient->deleteLater();
    activeOtaClient = nullptr;
}

void plcManager::startConf(const QString &name, const QString &yamlFilePath)
{
    qDebug()<<"plcManager::startConf for "<<name;
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
