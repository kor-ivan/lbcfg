#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QTextEdit>
#include "devicetreewidget.h"
#include "discoverwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    DeviceTreeWidget *treeWidget = nullptr;
    DiscoverWidget *m_discoverWidget = nullptr;
    // Теперь они доступны во всем классе
    QDockWidget *dock1 = nullptr;
    QDockWidget *dock2 = nullptr;
    // QDockWidget *dockConfig = nullptr;
    QMap<QString, QDockWidget*> configDocks;
    QTextEdit *configDisplay = nullptr;


    inline static const QColor alertColor = QColor(255, 205, 210);
    // bool SendDiscover;



protected:
    void showEvent(QShowEvent *event) override;


private slots:
    // void showTreeContextMenu(const QPoint &pos);
    void onDeviceSelected(
        const QString& ipv6,
        const QString& name);
    void getlbcfg (const QString &ipv6, const QString &name);

};
#endif // MAINWINDOW_H
