#include "discoverdockwidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QMenu>
#include <QClipboard>
#include <QSerialPort>
#include <QLabel>
#include <qlineedit.h>
#include <QMessageBox>
#include "logmanager.h"
#include "commandmanager.h"

DiscoverDockWidget::DiscoverDockWidget(QWidget *parent)
    : QDockWidget("Discover", parent), lbplc(plcManager::instanse())
{
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout *vbox = new QVBoxLayout(content);
    btnDiscover = new QPushButton(tr("Send Discover"), this);
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(0, 0, 0, 0);
    btnDiscover->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    vbox->setSpacing(0);
    hbox->addWidget(btnDiscover);

    initRtuPanel(hbox);

    hbox->addStretch();
    vbox->addLayout(hbox, 0);

    // Создаем таблицу
    table = new QTableWidget(this);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("IPv4"), tr("MAC"), tr("Delays (ms)"), tr("IF"), tr("RTU")});
    // Выделять строку целиком при клике на любую ячейку
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // разрешить выбирать только одну строку за раз
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    // запретить редактирование ячеек, чтобы они не открывались по двойному клику
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // table->setColumnHidden(6, true);
    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    // Остальные столбцы: подгоняем под содержимое текста
    for (int i = 1; i < table->columnCount(); ++i) {
        header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    header->setStyleSheet(
        "QHeaderView::section {"
        "    background-color: #f0f0f0;"
        "    font-weight: bold;"
        "    border: 1px solid #dcdcdc;"
        "    padding-left: 4px;" // Отступ только слева, чтобы текст не прилипал
        "    height: 20px;"      // Подсказка для высоты
        "}"
        );
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vbox->addWidget(table, 1);

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QTableView::customContextMenuRequested, this, &DiscoverDockWidget::showContextMenu);


    connect(
        btnDiscover,
        &QPushButton::clicked,
        this,
        &DiscoverDockWidget::triggerDiscover);

    connect(lbplc, &plcManager::discoverCompleted,
            this, &DiscoverDockWidget::discoverReceived);

    connect(
        table,
        &QTableWidget::cellDoubleClicked,
        this,
        &DiscoverDockWidget::onTableDoubleClicked);

    connect(lbplc, &plcManager::discoverStarting,
            this, &DiscoverDockWidget::cleanRow);

    connect(lbplc, &plcManager::connectionModeChanged, this, [this](plcManager::ConnectionMode mode) {
        setRtuMode(mode == plcManager::ConnectionMode::RTUMode);
    });

    setRtuMode(lbplc->connectionMode() == plcManager::ConnectionMode::RTUMode);
}

void DiscoverDockWidget::setRtuMode(bool enabled)
{
    rtuPanel->setVisible(enabled);
    btnDiscover->setText(enabled ? tr("Scan RTU") : tr("Send Discover"));
    setWindowTitle(enabled ? tr("Discover [RTU Mode]") : tr("Discover"));

    if (enabled) {
        startAddrSpin->setValue(m_rtuStartAddr);
        endAddrSpin->setValue(m_rtuEndAddr);
        timeoutSpin->setValue(m_rtuTimeOut);
        rebuildProfileCombo();
    }
    cleanRow();
}

QString DiscoverDockWidget::formatRtuProfile(const discover::ModbusRtuSettings &cfg)
{
    const QChar pChar = (cfg.parity == QSerialPort::EvenParity) ? QChar('E') :
                            (cfg.parity == QSerialPort::OddParity)  ? QChar('O') : QChar('N');
    return QString("%1/%2/%3/%4")
        .arg(cfg.baudRate)
        .arg(cfg.dataBits)
        .arg(pChar)
        .arg(cfg.stopBits);
}



void DiscoverDockWidget::cleanRow()
{
    table->clearContents();
    table->setRowCount(0); // Очищаем старые строки
}

void DiscoverDockWidget::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (QApplication::mouseButtons() != Qt::LeftButton) {
        return;
    }
    QTableWidgetItem *item = table->item(row, 0);
    if (!item) return;
    QString ipv6 = item->data(ipv6Role).toString();
    auto ctx = lbplc->getctx(ipv6, item->text());

    QTableWidgetItem *rtuItem = table->item(item->row(), 6);
    if (rtuItem)
        ctx.rtu = rtuItem->data(rtuRole).value<discover::ModbusRtuSettings>();
    emit deviceSelected(ctx);
}

void DiscoverDockWidget::showContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = table->itemAt(pos);
    if (!item) return;
    QString ipv6 = table->item(item->row(), 0)->data(ipv6Role).toString();

    auto ctx = lbplc->getctx(ipv6,
                             table->item(item->row(), 0)->text());

    QTableWidgetItem *rtuItem = table->item(item->row(), 6);
    if (rtuItem)
        ctx.rtu = rtuItem->data(rtuRole).value<discover::ModbusRtuSettings>();

    QMenu menu(this);
    QAction *AddDivice = menu.addAction(tr("Добавить"));
    // 2. Делаем его жирным
    QFont font = AddDivice->font();
    font.setBold(true);
    AddDivice->setFont(font);
    QAction *getConf = menu.addAction(tr("Запросить конфигурацию"));
    QAction *newConf = menu.addAction(tr("Создать новую конфигурацию"));
    QAction *copy = menu.addAction(tr("Копировать строку"));
    QAction *Allcopy = menu.addAction(tr("Копировать всё"));
    QAction *MacCopy = menu.addAction(tr("Копировать MAC"));
    QAction *ipv6Copy = menu.addAction(tr("Копировать IPv6"));
    menu.addSeparator();
    CommandManager::instance()->getUptimeAction(ctx, &menu);
    CommandManager::instance()->getRestartAction(ctx, &menu);
    CommandManager::instance()->getFlashAction(ctx, &menu);
    menu.addSeparator();
    CommandManager::instance()->getLogMenu(ctx, &menu);

    QAction *selectedItem = menu.exec(table->viewport()->mapToGlobal(pos));
    QClipboard *clipboard = QGuiApplication::clipboard();

    const auto &ldmap = plcManager::instanse()->getldmap();
    if (selectedItem == AddDivice){
        emit deviceSelected(ctx);
    }else if (selectedItem == copy) {
        clipboard->setText(ldmap.value(ctx.ipv6str()).toString());
    }else if (selectedItem == Allcopy) {
        QStringList qstr;
        for (const auto &i : ldmap) {
            qstr << i.toString();
        }
        clipboard->setText(qstr.join("\n"));
    }else if (selectedItem == getConf) {
        emit deviceSelected(ctx);
        emit requestConfig(ctx);
    }else if (selectedItem == newConf){
        emit newConfig(ctx);
    }else if (selectedItem == MacCopy){
        clipboard->setText(ldmap.value(ctx.ipv6str()).mac);
    }else if (selectedItem == ipv6Copy){
        clipboard->setText(ctx.ipv6str());
    }
}

// void DiscoverDockWidget::fillTable()
// {

// }

void DiscoverDockWidget::discoverReceived(const QMap<QString, discover::lbinfo> &DiscoverMap)
{
    const auto &ldmap = DiscoverMap;
    table->setSortingEnabled(false); // Отключаем сортировку на время вставки для скорости
    int row = 0;
    for (auto it = ldmap.begin(); it != ldmap.end(); ++it) {
        debugPLC() << it.key() << it.value();
        table->insertRow(row);
        QTableWidgetItem *nameItem = new QTableWidgetItem(it.value().name);
        nameItem->setData(ipv6Role, it.key());
        table->setItem(row, 0, nameItem);
        // table->setItem(row, 0, new QTableWidgetItem(it.value().name));
        table->setItem(row, 1, new QTableWidgetItem(it.value().type));
        table->setItem(row, 2, new QTableWidgetItem(it.value().ipv4));
        table->setItem(row, 3, new QTableWidgetItem(it.value().mac));
        QStringList strList;
        for (float val : it.value().delay)
            strList << QString::number(val);
        table->setItem(row, 4, new QTableWidgetItem(strList.join(",")));
        strList.clear();
        for (const auto& val : it.value().ifindex)
            strList << val;
        table->setItem(row, 5, new QTableWidgetItem(strList.join(",")));
        if (it.value().rtuSettings.addr > 0)
        {
            QTableWidgetItem *rtuItem = new QTableWidgetItem(QString("%1/%2/%3/%4/%5")
                                                                 .arg(it.value().rtuSettings.addr)
                                                                 .arg(it.value().rtuSettings.baudRate)
                                                                 .arg(it.value().rtuSettings.dataBits)
                                                                 .arg(it.value().rtuSettings.parity == QSerialPort::EvenParity ? "E" :
                                                                          it.value().rtuSettings.parity == QSerialPort::OddParity  ? "O" : "N")
                                                                 .arg(it.value().rtuSettings.stopBits));
            table->setItem(row, 6, rtuItem);
            rtuItem -> setData(rtuRole, QVariant::fromValue(it.value().rtuSettings));
        }

        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem *item = table->item(row, col);
            if (item) {
                item->setToolTip(it.key());
                if (it.value().btn) {
                    item->setBackground(alertColor);
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
            }
        }
        // table->setItem(row, 6, new QTableWidgetItem(it.key()));

        row++;
    }
    table->setSortingEnabled(true); // Возвращаем возможность сортировки
    logApp() << tr("Discover completed");
}

void DiscoverDockWidget::initRtuPanel(QHBoxLayout *parentLayout)
{
    rtuPanel = new QWidget(this);
    rtuPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QHBoxLayout *rtuLayout = new QHBoxLayout(rtuPanel);
    rtuLayout->setContentsMargins(8, 0, 0, 0);
    rtuLayout->setSpacing(6);

    auto fixVertical = [](QWidget *w) {
        w->setSizePolicy(w->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    };

    QLabel *lblAddr = new QLabel(tr("Адреса:"), rtuPanel);
    fixVertical(lblAddr);
    rtuLayout->addWidget(lblAddr);

    startAddrSpin = new QSpinBox(rtuPanel);
    startAddrSpin->setRange(1, 247);
    startAddrSpin->setValue(m_rtuStartAddr);
    fixVertical(startAddrSpin);
    rtuLayout->addWidget(startAddrSpin);

    QLabel *lblDash = new QLabel("—", rtuPanel);
    fixVertical(lblDash);
    rtuLayout->addWidget(lblDash);

    endAddrSpin = new QSpinBox(rtuPanel);
    endAddrSpin->setRange(1, 247);
    endAddrSpin->setValue(m_rtuEndAddr);
    fixVertical(endAddrSpin);
    rtuLayout->addWidget(endAddrSpin);

    QLabel *lblTimeout = new QLabel(tr("Таймаут:"), rtuPanel);
    fixVertical(lblTimeout);
    rtuLayout->addWidget(lblTimeout);

    timeoutSpin = new QSpinBox(rtuPanel);
    timeoutSpin->setRange(100, 10000);
    timeoutSpin->setSingleStep(50);
    timeoutSpin->setSuffix(tr(" мс"));
    timeoutSpin->setValue(m_rtuTimeOut);
    fixVertical(timeoutSpin);
    rtuLayout->addWidget(timeoutSpin);

    QLabel *lblProfiles = new QLabel(tr("RS-485:"), rtuPanel);
    fixVertical(lblProfiles);
    rtuLayout->addWidget(lblProfiles);

    profilesCombo = new QComboBox(rtuPanel);
    profilesCombo->setMinimumWidth(135);
    profilesModel = new QStandardItemModel(profilesCombo);
    profilesCombo->setModel(profilesModel);
    fixVertical(profilesCombo);
    rtuLayout->addWidget(profilesCombo);

    parentLayout->addWidget(rtuPanel);

    connect(startAddrSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (val > endAddrSpin->value()) {
            endAddrSpin->setValue(val);
        }
        m_rtuStartAddr = val;
    });
    connect(endAddrSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (val < startAddrSpin->value()) {
            startAddrSpin->setValue(val);
        }
        m_rtuEndAddr = val;
    });
    connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_rtuTimeOut = val;
    });
    connect(profilesModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
        if (!item) return;
        updateProfilesComboDisplay();
    });
}

void DiscoverDockWidget::rebuildProfileCombo()
{
    profilesModel->blockSignals(true);
    profilesModel->clear();

    const auto &profiles = lbplc->rtuProfiles();
    const auto curIt = lbplc->currentProfileIt();
    const int activeIdx = (curIt != profiles.cend())
                              ? static_cast<int>(std::distance(profiles.cbegin(), curIt))
                              : 0;

    for (int i = 0; i < profiles.size(); ++i) {
        auto *item = new QStandardItem(formatRtuProfile(profiles.at(i)));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(i == activeIdx ? Qt::Checked : Qt::Unchecked);
        profilesModel->appendRow(item);
    }

    profilesModel->blockSignals(false);
    updateProfilesComboDisplay();
}

void DiscoverDockWidget::updateProfilesComboDisplay()
{
    QStringList checkedLabels;
    for (int row = 0; row < profilesModel->rowCount(); ++row) {
        const QStandardItem *item = profilesModel->item(row);
        if (item && item->checkState() == Qt::Checked) {
            checkedLabels.append(item->text());
        }
    }

    QString displayText;
    if (checkedLabels.isEmpty()) {
        displayText = tr("Нет выбранных");
    } else if (checkedLabels.size() == 1) {
        displayText = checkedLabels.first();
    } else {
        displayText = QString("%1 (+%2)").arg(checkedLabels.first()).arg(checkedLabels.size() - 1);
    }

    if (profilesCombo->lineEdit() == nullptr) {
        profilesCombo->setEditable(true);
        profilesCombo->lineEdit()->setReadOnly(true);
    }
    profilesCombo->setCurrentText(displayText);
}

QList<discover::ModbusRtuSettings> DiscoverDockWidget::selectedRtuProfiles() const
{
    QList<discover::ModbusRtuSettings> result;
    const auto &profiles = lbplc->rtuProfiles();
    const auto &ports = lbplc->serialPorts();
    const auto portIt = lbplc->currentPortIt();
    const QString activePort = (portIt != ports.cend())
                                   ? portIt->portName()
                                   : QString();

    for (int row = 0; row < profilesModel->rowCount() && row < profiles.size(); ++row) {
        const QStandardItem *item = profilesModel->item(row);
        if (item && item->checkState() == Qt::Checked) {
            discover::ModbusRtuSettings cfg = profiles.at(row);
            cfg.portName = activePort;
            result.append(cfg);
        }
    }

    return result;
}

void DiscoverDockWidget::triggerDiscover()
{
    if (lbplc->connectionMode() == plcManager::ConnectionMode::RTUMode) {
        const auto profiles = selectedRtuProfiles();
        if (profiles.isEmpty()) {
            const QString msg = tr("Не выбран ни один профиль RS-485. Отметьте хотя бы один профиль перед сканированием.");
            logApp() << msg;
            QMessageBox::warning(this, tr("RTU Discover"), msg);
            return;
        }

        const int startAddr = m_rtuStartAddr;
        const int endAddr = m_rtuEndAddr;
        const int timeOut = m_rtuTimeOut;
        lbplc->startDiscoverRTU(profiles, startAddr, endAddr, timeOut);
    } else {
        lbplc->startDiscover();
    }
}

// QMap<QString, discover::lbinfo> DiscoverDockWidget::getLdmap() const
// {
//     return ldmap;
// }
