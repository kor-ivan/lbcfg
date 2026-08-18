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
    connect(lbc, &LBclient::lbDisconnect, this, [this, ctx]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
        debugApp() << "WatchSession::disconnect:" << m_key << ctx.ipv6;
        if (!message.isEmpty()) {
            emit watchErrorOccurred(message);
        }
        emit disconnected();
        lbc->deleteLater();
    });

    lbc->Execute();
}

WatchSession::~WatchSession()
{

}
