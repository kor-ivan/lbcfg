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
    void updateData(const QString &yamlText, const QString &name);
signals:

private:
    QTableView *varTableView = nullptr;
    QStandardItemModel *varModel = nullptr;
    QString decompose(const QList<QStringList> &data);

};

#endif // VARVIEW_H
