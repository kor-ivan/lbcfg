#ifndef DISCOVERDOCKWIDGET_H
#define DISCOVERDOCKWIDGET_H

#include <QObject>
#include <QDockWidget>
#include "plcmanager.h"
#include <QPushButton>
#include <QTableWidget>



class DiscoverDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DiscoverDockWidget(QWidget *parent = nullptr);

    QMap<QString, discover::lbinfo> getLdmap() const;

signals:
    void deviceSelected(const QString& ipv6, const QString& name);
    void requestConfig(const QString& ipv6, const QString& name);
    void newConfig(const QString& ipv6, const QString& name);

private slots:
    void cleanRow();
    void onTableDoubleClicked(int row, int column);
    void showContextMenu(const QPoint& pos);

private:
    plcManager *lbplc = nullptr;
    void discoverReceived(const QMap<QString, discover::lbinfo>& DiscoverMap);
    QPushButton* btnDiscover = nullptr;
    QTableWidget* table = nullptr;
    discover* wgtdiscover = nullptr;
    QMap<QString, discover::lbinfo> ldmap;

    bool discoverRunning = false;

    inline static const QColor alertColor =
        QColor(255,205,210);

};

#endif // DISCOVERDOCKWIDGET_H
