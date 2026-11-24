#ifndef DISCOVERDOCKWIDGET_H
#define DISCOVERDOCKWIDGET_H

#include <QObject>
#include <QDockWidget>
#include "plcmanager.h"
#include <QPushButton>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QStandardItemModel>


class DiscoverDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DiscoverDockWidget(QWidget *parent = nullptr);
    void setRtuMode(bool enabled);

    // QMap<QString, discover::lbinfo> getLdmap() const;
    static QString formatRtuProfile(const discover::ModbusRtuSettings &cfg);

signals:
    void deviceSelected(const plc::CommandContext &ctx);
    void requestConfig(const plc::CommandContext &ctx);
    void newConfig(const plc::CommandContext &ctx);

private slots:
    void cleanRow();
    void onTableDoubleClicked(int row, int column);
    void showContextMenu(const QPoint& pos);

private:
    enum DeviceRoles {
        ipv6Role = Qt::UserRole,
        rtuRole = Qt::UserRole + 1
    };
    plcManager *lbplc = nullptr;
    void discoverReceived(const QMap<QString, discover::lbinfo>& DiscoverMap);
    QPushButton* btnDiscover = nullptr;
    QTableWidget* table = nullptr;
    // discover* wgtdiscover = nullptr;
    // QMap<QString, discover::lbinfo> ldmap;

    bool discoverRunning = false;

    inline static const QColor alertColor =
        QColor(255,205,210);

    void initRtuPanel(QHBoxLayout *parentLayout);
    void rebuildProfileCombo();
    void updateProfilesComboDisplay();
    QList<discover::ModbusRtuSettings> selectedRtuProfiles() const;
    void triggerDiscover();
    void appendDeviceRow(const QString &key, const discover::lbinfo &info);
    void updateDiscoverButtonState();

    // Виджеты панели RTU режима
    QWidget *rtuPanel = nullptr;
    QComboBox *portCombo = nullptr;
    QComboBox *profilesCombo = nullptr;
    QStandardItemModel *profilesModel = nullptr;
    QSpinBox *startAddrSpin = nullptr;
    QSpinBox *endAddrSpin = nullptr;
    QSpinBox *timeoutSpin = nullptr;

    int m_rtuStartAddr = 1;
    // const int m_rtuEndAddrDefault = 10;
    int m_rtuEndAddr = 10;
    int m_rtuTimeOut = 1000;

};

#endif // DISCOVERDOCKWIDGET_H
