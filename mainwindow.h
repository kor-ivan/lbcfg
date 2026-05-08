#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QTreeView>
#include <QStandardItemModel>

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
    QTableWidget *table; // Чтобы иметь доступ к ней из методов
    QTreeView *treeView;
    QStandardItemModel *treeModel;

    inline static const QColor alertColor = QColor(255, 205, 210);
    bool SendDiscover;

protected:
    void showEvent(QShowEvent *event) override;


private slots:
    void btnDiscoverClicked(); // Слот для кнопки
    void onTableDoubleClicked(int row, int column);
};
#endif // MAINWINDOW_H
