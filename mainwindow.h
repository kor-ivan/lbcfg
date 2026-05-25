#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include<QTextEdit>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    // Теперь они доступны во всем классе
    QDockWidget *dock1;
    QDockWidget *dock2;
    QDockWidget *dockConfig = nullptr;
    QTableWidget *table; // Чтобы иметь доступ к ней из методов
    QTreeView *treeView;
    QTextEdit *configDisplay;
    QStandardItemModel *treeModel;

    inline static const QColor alertColor = QColor(255, 205, 210);
    bool SendDiscover;

    void getlbcfg (const QString &ipv6, const QString &name);

protected:
    void showEvent(QShowEvent *event) override;


private slots:
    void btnDiscoverClicked(); // Слот для кнопки
    void showTreeContextMenu(const QPoint &pos);
    void onDeviceSelected(
        const QString& ipv6,
        const QString& name);
};
#endif // MAINWINDOW_H
