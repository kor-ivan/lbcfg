#ifndef WATCHDOCKWIDGET_H
#define WATCHDOCKWIDGET_H

#include <QDockWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QPushButton>

class WatchDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    WatchDockWidget(const QString& name, QWidget *parent = nullptr);

    QString getPlcName() const;

    void setIpv6(const QString &newIpv6);

private:
    QString plcname;
    QString ipv6;
    QTableView *watch = nullptr;
    QStandardItemModel *watchModel = nullptr;

    void showIpEditDialog(QPushButton* anchorButton);
};

#endif // WATCHDOCKWIDGET_H
