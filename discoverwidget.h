#ifndef DISCOVERWIDGET_H
#define DISCOVERWIDGET_H

#include <QObject>
// #include <QWidget>
#include <QDockWidget>
#include <discover.h>
#include <QPushButton>
#include <QTableWidget>



class DiscoverWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DiscoverWidget(QWidget *parent = nullptr);

signals:
    void deviceSelected(const QString& ipv6,
                        const QString& name);

private slots:
    void startDiscover();
    void onTableDoubleClicked(int row, int column);

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

#endif // DISCOVERWIDGET_H
