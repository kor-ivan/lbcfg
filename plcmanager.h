#ifndef PLCMANAGER_H
#define PLCMANAGER_H

#include <QObject>
#include "lbprocess.h"
#include "discover.h"

class plcManager : public QObject
{
    Q_OBJECT
public:
    explicit plcManager(QObject *parent = nullptr);
    virtual ~plcManager();

    void scanDevice(const QString &ipv6, const QString &name);
    void requestConfig(const QString &ipv6, const QString &name);
    void startDiscover();
    void startFirmware(const QString &ipv6, const QString &filePath, int slot);
    void stopFirmware();

signals:
    void scanCompleted(const QString &ipv6, const QString &name, const QMap<qsizetype, lbprocess::scaninfo> &scanData);
    void configReceived(const QString &ipv6, const QString &name, const QString &yamlContent);
    void errorOccurred(const QString &message);
    void discoverCompleted(const QMap<QString, discover::lbinfo>& DiscoverMap);

    void firmwareStarted(const QString &ipv6);
    void firmwareProgressChanged(int prc);
    // void firmwareStatusChanged(const QString &message);
    void firmwareFinished();

private:
    static constexpr int port = 502;
    bool discoverRunning = false;
    LBclient *activeOtaClient = nullptr;
};

#endif // PLCMANAGER_H
