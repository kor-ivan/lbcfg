#include "watchsession.h"
#include "logmanager.h"


WatchSession::WatchSession(const plcManager::CommandContext &ctx, const QStringList &arg, QObject *parent)
    : QObject{parent}, m_key(ctx.name)
{
    debugApp() << "WatchSession::Starting process for:" << m_key << ctx.ipv6;
    QStringList m_arg = {"get"};
    m_arg.append(arg);
    lbc = new LBclient(this, m_arg);
    lbc->setTCPaddr(ctx.ipv6, 502);
    lbc->setTimeOut(t);

    connect(lbc, &LBclient::ExecuteCompleted, this, [this]
            (const QString& lbhost, const QStringList& result, const QString& message, const QModbusDevice::Error error){
        if (error == QModbusDevice::NoError)
            emit watchExeComleted(result);
        else
            emit watchErrorOccurred(QString("%1 -> %2").arg(m_key).arg(message));
    });
    connect(lbc, &LBclient::lbConnected, this, [this]
            (const QString& lbhost){
        debugApp() << "WatchSession::Connected:" << lbhost;
        m_connected = true;
        emit connected();
    });
    connect(lbc, &LBclient::lbDisconnect, this, [this, ctx]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
        debugApp() << "WatchSession::disconnect:" << m_key << ctx.ipv6 << message;
        if (!message.isEmpty()) {
            emit watchErrorOccurred(message);
        }
        m_connected = false;
        emit disconnected();
        // lbc->deleteLater();
    });
}

WatchSession::~WatchSession()
{

}

void WatchSession::start()
{
    if (lbc)
        lbc->Execute();
}

void WatchSession::stop()
{
    if (lbc)
        lbc->lbDisconnectDevice();
}

bool WatchSession::isConnected() const
{
    return m_connected;
}
