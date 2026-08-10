#ifndef VARVIEW_H
#define VARVIEW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>

class varView : public QWidget
{
    Q_OBJECT
public:
    explicit varView(QWidget *parent = nullptr);
    void updateData(const QString &yamlText);
signals:

private:
    QTableView *varTableView = nullptr;
    QStandardItemModel *varModel = nullptr;
};

#endif // VARVIEW_H
