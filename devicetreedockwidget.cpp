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
    connect(treeView, &QTreeView::doubleClicked, this,
            [this](const QModelIndex &index){
                if (!index.isValid()) return;
                if (!index.parent().isValid())
                    emit requestConfig(index.data(Qt::UserRole).toString(),
                                       index.data().toString());
            }
            );
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
        plcRoot->setText(name);
        rootIndex = plcRoot->index();
    }

    // Итерируем по результатам сканирования
    for (auto it = scan.begin(); it != scan.end(); ++it) {
        const auto &info = it.value();
        // Создаем элементы для двух колонок
        QStandardItem *col1 = new QStandardItem(QString("Slot %1: %2").arg(it.key()).arg(info.devtype));
        col1->setData(it.key(), Qt::UserRole);
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

bool DeviceTreeDockWidget::containsName(const QString &name)
{
    qDebug()<<"into DeviceTreeDockWidget::contains "<<name;
    for (int i = 0; i < treeModel->rowCount(); ++i) {
        auto *item = treeModel->item(i);
        if (item->text() == name)
            return true;
    }
    return false;
}

void DeviceTreeDockWidget::showContextMenu(const QPoint &pos)
{
    // Получаем индекс элемента, на который кликнули
    QModelIndex index = treeView->indexAt(pos);
    if (!index.isValid()) return;
    bool isRoot = !index.parent().isValid();
    CommandContext ctx;
    if (isRoot) {
        ctx.name = index.data().toString();
        ctx.ipv6 = index.data(Qt::UserRole).toString();
    } else {
        ctx.name = index.parent().data().toString();
        ctx.ipv6 = index.parent().data(Qt::UserRole).toString();
        ctx.slot = index.data(Qt::UserRole).toInt();
    }

    QMenu menu(this);
    // --- Только для Устройства ---
    if (isRoot) {
        QAction *getConfigAction = menu.addAction(QString("Запросить конфигурацию у %1").arg(ctx.name));
        QFont font = getConfigAction->font();
        font.setBold(true);
        getConfigAction->setFont(font);
        connect(getConfigAction, &QAction::triggered, this, [this, ctx]() {
            emit requestConfig(ctx.ipv6, ctx.name);
        });

        QAction *removeAction = menu.addAction(QString("Удалить %1").arg(ctx.name));
        connect(removeAction, &QAction::triggered, this, [this, index]() {
            treeModel->removeRow(index.row());
        });

        QAction *restartAll = menu.addAction("Перезагрузить все");
        connect(restartAll, &QAction::triggered, this,
                [this, ctx](){
            qDebug()<<"Перезагрузить все";
                    LBclient *lbc = new LBclient (this);
                    lbc->setTCPaddr(ctx.ipv6, 502);
                        //connect disconnect event
                    lbprocess *prc = new lbprocess(this, lbc);
                    connect(prc, &lbprocess::outMessage, this, [this, prc]
                            (const QString &lbstr, const QString &message, const QModbusDevice::Error error){
                                if (message == "END"){
                                    prc->deleteLater();
                                    qDebug()<<message;
                                }
                            }
                            );
                    prc->run(lbprocess::restartall);
                });

        QAction *fsformat = menu.addAction("Сбросить к заводским");
        connect(fsformat, &QAction::triggered, this, [this, ctx](){
            auto reply = QMessageBox::question(this,
                                               "Подтверждение сброса",
                                               QString("Вы уверены, что хотите сбросить устройство %1 к заводским настройкам?").arg(ctx.name),
                                               QMessageBox::Yes | QMessageBox::No,
                                               QMessageBox::No); // Кнопка по умолчанию
            if (reply == QMessageBox::Yes)
                lbc_executeCommand(ctx, {"fsformat"}, "Сброс к заводским", [ctx, this] (const QStringList& res){
                    return QString("%1 сброшено к заводским настройкам. Требуется перезагрузка.")
                        .arg(ctx.name);
                });
        });
    }
    // --- Общие действия ---
    QAction *getUptime = menu.addAction("Время работы");
    connect(getUptime, &QAction::triggered, this, [this, ctx]() {
        lbc_executeCommand(ctx, {"get", "sys.uptime"}, "Время работы", [ctx, this](const QStringList& res) {
            QString uptime = res.isEmpty() ? toBold("none") : toBold(res.at(0));
            return QString("Время работы %1 %2 сек").arg(ctx.displayName(), uptime);
        });
    });
    QAction *restart = menu.addAction("Перезагрузить");
    connect(restart, &QAction::triggered, this, [this, ctx]() {
        lbc_executeCommand(ctx, {"set", "sys.restart=1"}, "Перезагрузка", [ctx](const QStringList&) {
            return QString("Команда на перезагрузку %1 отправлена").arg(ctx.displayName());
        });
    });

    menu.exec(treeView->viewport()->mapToGlobal(pos));

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

QString DeviceTreeDockWidget::toBold(const QString &text)
{
    return QString("<b>%1</b>").arg(text);
}
