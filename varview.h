#ifndef VARVIEW_H
#define VARVIEW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include "lbyaml.h"

class varView : public QWidget
{
    Q_OBJECT
public:
    explicit varView(QWidget *parent = nullptr);
    // void updateData(const QString &yamlText, const QString &name);
    void updateData(lbyaml *parser);
    QMap<QString, lbyaml::lbvar> getUpdatedData();
    bool isModified() const;
    void resetModified();

signals:
    void onChanged();
    void addVariableToWatch(const QString &varName);


protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QTableView *varTableView = nullptr;
    QStandardItemModel *varModel = nullptr;
    QString decompose(const QList<QStringList> &data);
    QMap<QString, lbyaml::lbvar> m_lbVarMap;
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    bool modified = false;
    void showContextMenu(const QPoint &pos);
    QPoint m_dragStartPos;
};

#endif // VARVIEW_H
