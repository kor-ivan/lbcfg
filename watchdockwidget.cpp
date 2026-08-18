#include "watchdockwidget.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLineEdit>
#include "logmanager.h"
#include "plcmanager.h"

WatchDockWidget::WatchDockWidget(const QString &name, QWidget *parent)
    : QDockWidget{QString("Watch: %1").arg(name), parent}, plcname(name)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const int elementHeight = 22;
    QPushButton *addBtn = new QPushButton(this);
    addBtn->setFixedSize(elementHeight, elementHeight);
    addBtn->setToolTip("Добавить переменную вручную");
    addBtn->setText("+");

    QPushButton *remBtn = new QPushButton(this);
    remBtn->setFixedSize(elementHeight, elementHeight);
    remBtn->setToolTip("Удалить выбранную переменную");
    remBtn->setText("-");

    QPushButton *ipBtn = new QPushButton(this);
    ipBtn->setFixedSize(elementHeight, elementHeight);
    ipBtn->setToolTip("Указать IP адрес");
    ipBtn->setText("IP");

    QPushButton *connBtn = new QPushButton(this);
    connBtn->setFixedHeight(elementHeight);
    connBtn->setFixedWidth(60); // Делаем чуть шире, чтобы текст "Connect" помещался
    connBtn->setToolTip("Подключиться к PLC для отладки");
    connBtn->setText("Connect");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(2);
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(remBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(connBtn);
    buttonLayout->addWidget(ipBtn);

    layout->addLayout(buttonLayout);

    watch = new QTableView(this);
    watch->setShowGrid(true);
    watch->setSelectionBehavior(QAbstractItemView::SelectRows);
    watch->setSelectionMode(QAbstractItemView::SingleSelection);
    watch->setAlternatingRowColors(true);

    QHeaderView *header = watch->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setVisible(true);

    QHeaderView *v_header = watch->verticalHeader();
    v_header->setDefaultSectionSize(22);
    v_header->setVisible(true);

    watchModel = new QStandardItemModel(this);
    watch->setModel(watchModel);
    watchModel->setHorizontalHeaderLabels({"var name","value"});

    layout->addWidget(watch);
    setWidget(container);

    connect(addBtn, &QPushButton::clicked, this, [this](){
        QList<QStandardItem*> rowItems;
        rowItems.append(new QStandardItem());
        rowItems.append(new QStandardItem());
        watchModel->appendRow(rowItems);
    });

    connect(ipBtn, &QPushButton::clicked, this, [this, ipBtn](){
        showIpEditDialog(ipBtn);
    });

    connect(remBtn, &QPushButton::clicked, this, [this](){
        QModelIndex currentIndex = watch->currentIndex();
        if (currentIndex.isValid()) {
            watchModel->removeRow(currentIndex.row());
        }
    });

    connect(connBtn, &QPushButton::clicked, this, [this, connBtn](){
        toggleConnection(connBtn);
    });

}

QString WatchDockWidget::getPlcName() const
{
    return plcname;
}

void WatchDockWidget::setIpv6(const QString &newIpv6)
{
    ipv6 = newIpv6;
    debugApp()<<"WatchDockWidget set IP:"<<ipv6;
}

void WatchDockWidget::showIpEditDialog(QPushButton *anchorButton)
{
    QLineEdit *ipEdit = new QLineEdit(this);
    ipEdit->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    ipEdit->setAttribute(Qt::WA_DeleteOnClose);
    ipEdit->setText(this->ipv6);
    ipEdit->setPlaceholderText("Enter IP...");
    ipEdit->setMinimumWidth(150);

    // ipEdit->setStyleSheet("QLineEdit { border: 1px solid #999; padding: 2px; background: white; }");

    QPoint globalPos = anchorButton->mapToGlobal(QPoint(0, 0));
    ipEdit->setGeometry(globalPos.x(), globalPos.y(), ipEdit->minimumWidth(), anchorButton->height());

    connect(ipEdit, &QLineEdit::returnPressed, this, [this, ipEdit](){
        QString text = ipEdit->text().trimmed();
        if (!text.isEmpty()) {
            this->setIpv6(text);
            // Здесь можно вызвать сигнал или лог, что IP изменен
        }
        ipEdit->close();
    });

    ipEdit->show();
    ipEdit->setFocus();
    ipEdit->selectAll();
}

void WatchDockWidget::toggleConnection(QPushButton *connBtn)
{
    plcManager::CommandContext ctx;
    ctx.ipv6 = ipv6;
    ctx.name = plcname;

    QStringList param;
    for (int row = 0; row < watchModel->rowCount(); ++row) {
        QStandardItem *item = watchModel->item(row, 0); // Получаем ячейку первого столбца
        if (item && !item->text().isEmpty()) {
            param.append(item->text());
        }
    }

    session = plcManager::instanse()->startWatch(ctx, param);

    connect(session, &WatchSession::watchExeComleted, this, [this]
            (const QStringList &data){
                debugApp()<<"data recive:"<<data;

                for (int i = 0; i < data.size(); ++i) {
                    // Проверяем, что строка все еще существует в модели (на случай рефакторинга таблицы на лету)
                    if (i < watchModel->rowCount()) {
                        QStandardItem *valueItem = watchModel->item(i, 1); // Проверяем ячейку во втором столбце

                        if (!valueItem) {
                            // Если ячейки еще нет — создаем её
                            valueItem = new QStandardItem();
                            watchModel->setItem(i, 1, valueItem);
                        }

                        // Записываем новое значение
                        valueItem->setText(data.at(i));
                    }
                }
            });
}

