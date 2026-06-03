#include "devicetreedockwidget.h"
#include <QVBoxLayout>
#include <QMenu>

DeviceTreeDockWidget::DeviceTreeDockWidget(QWidget *parent)
    : QDockWidget("Tree View", parent)
{
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout *layout = new QVBoxLayout(content);
    treeView = new QTreeView(this);
    treeModel = new QStandardItemModel(this);
    treeView->setModel(treeModel);
    treeView->setHeaderHidden(true);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested,
            this, &DeviceTreeDockWidget::showContextMenu);
    layout->addWidget(treeView);
}

void DeviceTreeDockWidget::updateDevice(const QString &ipv6, const QString &name, const QMap<qsizetype, lbprocess::scaninfo> &scan)
{
    QStandardItem* plcRoot = findPlcRoot(ipv6);
    QModelIndex rootIndex;

    // Если не нашли, создаем новый корень
    if (!plcRoot) {
        plcRoot = new QStandardItem(name);
        plcRoot->setData(ipv6, Qt::UserRole); // Прячем ID для поиска в будущем
        QFont rootFont = plcRoot->font();
        rootFont.setBold(true);
        rootFont.setPointSize(rootFont.pointSize());
        plcRoot->setFont(rootFont);
        treeModel->appendRow(plcRoot);
        rootIndex = treeModel->index(treeModel->rowCount() - 1, 0);
    }else{
        plcRoot->removeRows(0, plcRoot->rowCount());
        rootIndex = plcRoot->index();
    }

    // Итерируем по результатам сканирования
    for (auto it = scan.begin(); it != scan.end(); ++it) {
        const auto &info = it.value();
        // Создаем элементы для двух колонок
        QStandardItem *col1 = new QStandardItem(QString("Slot %1: %2").arg(it.key()).arg(info.devtype));
        // 2. Делаем их жирными
        QFont boldFont = col1->font();
        boldFont.setBold(true);
        col1->setFont(boldFont);
        if (info.master)
            col1->setText(col1->text() + " [MASTER]");
        // Добавляем их в модель как одну строку
        plcRoot->appendRow(col1);
        // Теперь добавляем подробности ВНУТРЬ (как подветки)
        col1->appendRow(new QStandardItem("MAC: " + info.mac));
        col1->appendRow(new QStandardItem("Version: " + info.version));
        col1->appendRow(new QStandardItem("Serial: " + info.data.at(0)));
    }

    // Раскрываем дерево
    treeView->expand(rootIndex);
}

void DeviceTreeDockWidget::showContextMenu(const QPoint &pos)
{
    // Получаем индекс элемента, на который кликнули
    QModelIndex index = treeView->indexAt(pos);
    if (!index.isValid() || index.parent().isValid()) return;

    QString name = index.data().toString();
    QString ipv6 = index.data(Qt::UserRole).toString();

    QMenu menu(this);
    QAction *removeAction = menu.addAction(QString("Удалить %1").arg(name));
    QAction *getConfigAction = menu.addAction(QString("Запросить конфигурацию у %1").arg(name));

    // Вызываем меню в позиции курсора
    QAction *selectedItem = menu.exec(treeView->viewport()->mapToGlobal(pos));

    if (selectedItem == removeAction) {
        // Удаляем строку из модели
        treeModel->removeRow(index.row());
    }else if (selectedItem == getConfigAction){
        emit requestConfig(ipv6,name);
    }
}

QStandardItem *DeviceTreeDockWidget::findPlcRoot(const QString &ipv6)
{
    for (int i = 0; i < treeModel->rowCount(); ++i) {
        auto *item = treeModel->item(i);
        if (item->data(Qt::UserRole).toString() == ipv6)
            return item;
    }
    return nullptr;
}
