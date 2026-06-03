#ifndef DISCOVERDOCKWIDGET_H
#define DISCOVERDOCKWIDGET_H

#include <QObject>
#include <QDockWidget>
#include <discover.h>
#include <QPushButton>
#include <QTableWidget>



class DiscoverDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DiscoverDockWidget(QWidget *parent = nullptr);

signals:
    void deviceSelected(const QString& ipv6,
                        const QString& name);

private slots:
    void startDiscover();
    void onTableDoubleClicked(int row, int column);
    void showContextMenu(const QPoint& pos);

private:
    void fillTable(
        const QMap<QString, discover::lbinfo>& discoverMap);

    QPushButton* btnDiscover = nullptr;
    QTableWidget* table = nullptr;
    discover* wgtdiscover = nullptr;

    bool discoverRunning = false;

    inline static const QColor alertColor =
        QColor(255,205,210);

};

#endif // DISCOVERDOCKWIDGET_H
