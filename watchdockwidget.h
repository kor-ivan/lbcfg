#ifndef WATCHDOCKWIDGET_H
#define WATCHDOCKWIDGET_H

#include <QDockWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include "watchsession.h"

class WatchDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    WatchDockWidget(const QString& name, QWidget *parent = nullptr);

    QString getPlcName() const;

    void setIpv6(const QString &newIpv6);
    void addVar(const QString &varName = QString());
    void toggleConnection();

private:
    QString plcname;
    QString ipv6;
    QTableView *watch = nullptr;
    QStandardItemModel *watchModel = nullptr;

    void showIpEditDialog(QPushButton* anchorButton);

    QPointer<WatchSession> session;
    void receiveData(const QStringList &data);

    QStringList collectVariables() const;
    void updateSessionVariables();

    QDoubleSpinBox *intervalSpin = nullptr;
    void showContextMenu(const QPoint& pos);

    QSet<QString> forcedVar;

    void updateTableColors();

    QPushButton *connBtn = nullptr;



};

#endif // WATCHDOCKWIDGET_H
