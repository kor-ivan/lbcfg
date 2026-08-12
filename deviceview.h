#ifndef DEVICEVIEW_H
#define DEVICEVIEW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>

class deviceView : public QWidget
{
    Q_OBJECT
public:
    explicit deviceView(QWidget *parent = nullptr);

signals:

private:
    QTableView *deviceTableView = nullptr;
    QStandardItemModel *deviceModel = nullptr;

};

#endif // DEVICEVIEW_H
