#ifndef WATCHSESSION_H
#define WATCHSESSION_H

#include <QObject>
#include "lbclient.h"
#include "plcmanager.h"


class WatchSession : public QObject
{
    Q_OBJECT
public:
    explicit WatchSession(const plcManager::CommandContext &ctx, const QStringList &arg, QObject *parent = nullptr);
    virtual ~WatchSession();


signals:
    void watchExeComleted(const QStringList &data);
    void watchErrorOccurred(const QString &message);
    void disconnected();

private:
    LBclient *lbc = nullptr;
    QString m_key;
    int t = 1000;

};

#endif // WATCHSESSION_H
