#include "watchdockwidget.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QApplication>
#include <qstyleditemdelegate.h>
#include "logmanager.h"
#include "plcmanager.h"

enum class DisplayFormat {
    Default,
    Hex,
    Binary,
    Dec
};

constexpr int DisplayFormatRole = Qt::UserRole + 1;
constexpr int IsUnsignedRole   = Qt::UserRole + 2;

class ValueDisplayDelegate : public QStyledItemDelegate
{
public:
    explicit ValueDisplayDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QString displayText(const QVariant &value, const QLocale &locale) const override {
        return QStyledItemDelegate::displayText(value, locale);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        // Извлекаем формат конкретной строки из первой колонки (var name) текущего ряда
        QModelIndex varNameIndex = index.siblingAtColumn(0);
        DisplayFormat format = static_cast<DisplayFormat>(varNameIndex.data(DisplayFormatRole).toInt());

        if (format != DisplayFormat::Default && index.data().canConvert<qlonglong>()) {
            bool ok = false;
            qlonglong num = index.data().toLongLong(&ok);
            if (ok) {
                QStyleOptionViewItem opt = option;
                initStyleOption(&opt, index);

                bool isUnsigned = varNameIndex.data(IsUnsignedRole).toBool();

                int pad = 8;
                if (isUnsigned) {
                    qulonglong uNum = static_cast<qulonglong>(num);

                    // Восстанавливаем исходное знаковое число из 64-битного эха,
                    // чтобы понять, в какую сетку оно изначально закладывалось в ПЛК.
                    qlonglong s8  = static_cast<char>(uNum & 0xFFULL);
                    qlonglong s16 = static_cast<short>(uNum & 0xFFFFULL);
                    qlonglong s32 = static_cast<int>(uNum & 0xFFFFFFFFULL);

                    // 1. Проверяем, совпадает ли наше 64-битное число с 8-битным знаковым расширением
                    if (static_cast<qulonglong>(s8) == uNum || uNum <= 0xFFULL) {
                        pad = 8;
                    }
                    // 2. Проверяем, совпадает ли с 16-битным знаковым расширением (Наш случай для -255)
                    else if (static_cast<qulonglong>(s16) == uNum || uNum <= 0xFFFFULL) {
                        pad = 16;
                    }
                    // 3. Проверяем, совпадает ли с 32-битным знаковым расширением
                    else if (static_cast<qulonglong>(s32) == uNum || uNum <= 0xFFFFFFFFULL) {
                        pad = 32;
                    }
                    // 4. Во всех остальных случаях — это честное 64-битное беззнаковое число
                    else {
                        pad = 64;
                    }
                } else {
                    if (num < -2147483648LL || num > 2147483647LL) pad = 64;
                    else if (num < -32768 || num > 32767)           pad = 32;
                    else if (num < -128 || num > 127)               pad = 16;
                }

                if (format == DisplayFormat::Dec) {
                    if (isUnsigned) {
                        qulonglong uNum = static_cast<qulonglong>(num);
                        if (pad == 8)  uNum &= 0xFF;
                        if (pad == 16) uNum &= 0xFFFF;
                        if (pad == 32) uNum &= 0xFFFFFFFF;
                        opt.text = QString::number(uNum);
                    } else {
                        qlonglong sNum = num;
                        if (pad == 8)  sNum = static_cast<char>(num);
                        if (pad == 16) sNum = static_cast<short>(num);
                        if (pad == 32) sNum = static_cast<int>(num);
                        opt.text = QString::number(sNum);
                    }
                }
                else if (format == DisplayFormat::Hex) {
                    qulonglong uNum = static_cast<qulonglong>(num);
                    if (pad == 8)  uNum &= 0xFF;
                    if (pad == 16) uNum &= 0xFFFF;
                    if (pad == 32) uNum &= 0xFFFFFFFF;

                    opt.text = QString("0x%1").arg(QString::number(uNum, 16).toUpper().rightJustified(pad / 4, '0'));
                }
                else if (format == DisplayFormat::Binary) {
                    qulonglong uNum = static_cast<qulonglong>(num);
                    QString bin = QString::number(uNum, 2);

                    if (!isUnsigned && num < 0) {
                        // Для отрицательных знаковых чисел берем честный дополнительный код нужной длины
                        bin = bin.right(pad);
                    } else {
                        // Для беззнаковых и положительных маскируем и дополняем нулями
                        if (pad == 8)  uNum &= 0xFF;
                        if (pad == 16) uNum &= 0xFFFF;
                        if (pad == 32) uNum &= 0xFFFFFFFF;
                        bin = QString::number(uNum, 2).rightJustified(pad, '0');
                    }
                    opt.text = QString("0b%1").arg(bin);
                }
                QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
                return;
            }
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit) {
            QStyledItemDelegate::setModelData(editor, model, index);
            return;
        }
        QString text = lineEdit->text().trimmed().toLower();
        if (text.isEmpty()) {
            QStyledItemDelegate::setModelData(editor, model, index);
            return;
        }

        QModelIndex varNameIndex = index.siblingAtColumn(0);
        DisplayFormat format = static_cast<DisplayFormat>(varNameIndex.data(DisplayFormatRole).toInt());
        bool isUnsigned = varNameIndex.data(IsUnsignedRole).toBool();

        bool ok = false;
        qlonglong parsedValue = 0;
        DisplayFormat expectedFormat = DisplayFormat::Default;

        auto parseAsFormat = [&](QString str, int base) -> qlonglong {
            if (isUnsigned) {
                bool uOk = false;
                qulonglong uVal = str.toULongLong(&uOk, base);
                ok = uOk;
                return static_cast<qlonglong>(uVal);
            } else {
                return str.toLongLong(&ok, base);
            }
        };

        if (text.startsWith("0x")) {
            expectedFormat = DisplayFormat::Hex;
            parsedValue = parseAsFormat(text.mid(2), 16);
        } else if (text.startsWith("0b")) {
            expectedFormat = DisplayFormat::Binary;
            parsedValue = parseAsFormat(text.mid(2), 2);
        } else {
            expectedFormat = format;
            switch (format) {
            case DisplayFormat::Hex:    parsedValue = parseAsFormat(text, 16); break;
            case DisplayFormat::Binary: parsedValue = parseAsFormat(text, 2); break;
            case DisplayFormat::Dec:    parsedValue = parseAsFormat(text, 10); break;
            default: break;
            }
        }

        if (ok) {
            model->setData(index, QString::number(parsedValue), Qt::EditRole);
        }
        else if (expectedFormat != DisplayFormat::Default) {
            auto formatToString = [](DisplayFormat fmt) -> QString {
                if (fmt == DisplayFormat::Hex) return "HEX";
                if (fmt == DisplayFormat::Binary) return "Binary";
                return "Unknown";
            };
            QMessageBox::warning(editor->window(), tr("Ошибка ввода"),
                                 tr("Введенное значение не соответствует выбранному формату (%1).\nИзменения отменены.").arg(formatToString(expectedFormat)));
        } else {
            QStyledItemDelegate::setModelData(editor, model, index);
        }
    }
};

WatchDockWidget::WatchDockWidget(const plc::CommandContext &ctx, QWidget *parent)
    : QDockWidget{QString("Watch: %1").arg(ctx.name), parent}, m_ctx(ctx)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const int elementHeight = 22;
    QPushButton *addBtn = new QPushButton(this);
    addBtn->setFixedSize(elementHeight, elementHeight);
    addBtn->setToolTip(tr("Добавить переменную вручную"));
    addBtn->setText("+");

    QPushButton *remBtn = new QPushButton(this);
    remBtn->setFixedSize(elementHeight, elementHeight);
    remBtn->setToolTip(tr("Удалить выбранную переменную"));
    remBtn->setText("-");

    ipBtn = new QPushButton(this);
    ipBtn->setFixedSize(elementHeight, elementHeight);
    ipBtn->setToolTip(QString(tr("Указать IP адрес\n%1")).arg(ctx.ipv6.toString()));
    ipBtn->setText("IP");

    intervalSpin = new QDoubleSpinBox(this);
    intervalSpin->setFixedHeight(elementHeight);
    intervalSpin->setRange(0.1, 10.0);        // Мин и макс значения
    intervalSpin->setValue(1.0);               // Значение по умолчанию (1 секунда)
    intervalSpin->setSingleStep(0.1);          // Начальный шаг
    intervalSpin->setDecimals(1);              // Один знак после запятой (для 0.1)
    intervalSpin->setSuffix(tr(" s", "seconds"));             // Красивый суффикс "s" (секунды)
    intervalSpin->setToolTip(tr("Интервал опроса"));
    intervalSpin->setEnabled(false);

    connBtn = new QPushButton(this);
    connBtn->setFixedHeight(elementHeight);
    connBtn->setMinimumWidth(70);
    connBtn->setToolTip(tr("Подключиться к PLC для отладки"));
    connBtn->setText(tr("Connect"));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(2);
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(remBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(connBtn);
    buttonLayout->addWidget(intervalSpin);
    buttonLayout->addWidget(ipBtn);

    layout->addLayout(buttonLayout);

    watch = new QTableView(this);
    watch->setShowGrid(true);
    watch->setSelectionBehavior(QAbstractItemView::SelectRows);
    watch->setSelectionMode(QAbstractItemView::SingleSelection);
    watch->setAlternatingRowColors(true);
    watch->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(watch, &QTableView::customContextMenuRequested,
            this, &WatchDockWidget::showContextMenu);
    // Разрешаем сброс данных на саму таблицу watch
    watch->setAcceptDrops(true);
    // Устанавливаем фильтр событий на таблицу, чтобы перехватывать Drag & Drop
    watch->installEventFilter(this);
    valueDelegate = new ValueDisplayDelegate(this);
    watch->setItemDelegateForColumn(1, valueDelegate); // value
    watch->setItemDelegateForColumn(2, valueDelegate); // set value
    watch->setItemDelegateForColumn(3, valueDelegate); // force value


    QHeaderView *header = watch->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setVisible(true);

    QHeaderView *v_header = watch->verticalHeader();
    v_header->setDefaultSectionSize(22);
    v_header->setVisible(true);

    watchModel = new QStandardItemModel(this);
    watch->setModel(watchModel);
    watchModel->setHorizontalHeaderLabels({tr("var name"),tr("value"),tr("set value"), tr("force value")});

    layout->addWidget(watch);
    setWidget(container);

    connect(addBtn, &QPushButton::clicked, this, [this](){
        addVar();
    });

    connect(ipBtn, &QPushButton::clicked, this, [this](){
        showIpEditDialog(ipBtn);
    });

    connect(remBtn, &QPushButton::clicked, this, [this](){
        QModelIndex currentIndex = watch->currentIndex();
        if (currentIndex.isValid()) {
            watchModel->removeRow(currentIndex.row());
            updateSessionVariables();
        }
    });

    connect(connBtn, &QPushButton::clicked, this, [this](){
        toggleConnection();
    });

    connect(watchModel, &QStandardItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        // Проверяем, что изменилась именно первая колонка (var name)
        if (topLeft.column() == 0) {
            updateSessionVariables();
            if (session && session->isConnected())
                updateTableColors();
            // Проверяем, что изменилась именно третья колонка (set value)
        }else if (topLeft.column() == 2){
            int row = topLeft.row();
            QString val = topLeft.data().toString().trimmed();
            if (val.isEmpty()) return;
            QStringList get = {"get"};
            get.append(collectVariables());

            QStringList set = {"set", QString("%1=%2")
                                          .arg(watchModel->item(row, 0)->text(), val)};
            if (session && session->isConnected())
            {
                session->setQuery({get, set});
                debugApp() << tr("WatchDockWidget: list of variables for recording:") << get << set;
                watchModel->blockSignals(true);
                watchModel->item(row, 2)->setText("");
                watchModel->blockSignals(false);
            }
        }else if (topLeft.column() == 3){
            QStringList get = {"get"};
            get.append(collectVariables());
            int row = topLeft.row();
            QString varName = watchModel->item(row, 0)->text();
            QString val = topLeft.data().toString().trimmed();
            if (val != "")
            {
                QStringList force = {"force", QString("%1=%2")
                                                  .arg(varName, val)};
                if (session && session->isConnected()){
                    session->setQuery({get, force});
                    debugApp() << tr("WatchDockWidget: list of variables for FORCE:") << get << force;
                    forcedVar.insert(varName);
                    updateTableColors();
                }
            }
        }
    });

    static double lastValue = 1.0;
    connect(intervalSpin, &QDoubleSpinBox::valueChanged, this, [this](double value){
        intervalSpin->blockSignals(true);

        if (qFuzzyCompare(lastValue, 1.0)){
            if (value > 1.0){
                intervalSpin->setSingleStep(1.0);
                intervalSpin->setValue(2.0);
            }else{
                intervalSpin->setSingleStep(0.1);
                intervalSpin->setValue(0.9);
            }
        }

        lastValue = intervalSpin->value();

        intervalSpin->blockSignals(false);
        if (session) {
            int msec = static_cast<int>(intervalSpin->value() * 1000);
            session->setTimeOut(msec);
            debugApp() << tr("The survey interval has been changed to:") << msec << tr("ms");
        }
    });
}


QString WatchDockWidget::getPlcName() const
{
    return m_ctx.name;
}

void WatchDockWidget::setIpv6(const QString &newIpv6)
{
    QStringList splitstr = newIpv6.split('/');
    auto h = QHostAddress(splitstr.value(0));
    if (h.isNull()) return;
    auto m_plcm = plcManager::instanse();
    if (h.protocol() == QAbstractSocket::IPv6Protocol){
        m_ctx = m_plcm->getctx(splitstr.value(0), m_ctx.name);
    }else if(h.protocol() == QAbstractSocket::IPv4Protocol){
        m_ctx = m_plcm->getctx(splitstr.value(0), m_ctx.name);
    }
    if (!splitstr.value(1).isEmpty()){
        bool ok;
        int slot = splitstr.value(1).toInt(&ok);
        if (ok) m_ctx.slot = slot;
    }
    ipBtn->setToolTip(tr("Указать IP адрес") +
                      (m_ctx.ipv6str().isEmpty() ? "" : "\n" + m_ctx.ipv6str()) +
                      (m_ctx.isSlot() ? "\nslot " + QString::number(m_ctx.slot) : ""));
    debugApp()<<"WatchDockWidget set IP:"<<m_ctx.ipv6str()<<"slot"<<m_ctx.slot;
}

void WatchDockWidget::showIpEditDialog(QPushButton *anchorButton)
{
    QLineEdit *ipEdit = new QLineEdit(this);
    ipEdit->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    ipEdit->setAttribute(Qt::WA_DeleteOnClose);
    ipEdit->setText(m_ctx.ipv6str());
    ipEdit->setPlaceholderText(tr("Enter IP..."));
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

void WatchDockWidget::toggleConnection()
{
    if (session && session->isConnected()){
        session->stop();
        return;
    }

    // plc::CommandContext ctx;
    // ctx.ipv6 = ipv6;
    // ctx.name = plcname;

    QStringList param = collectVariables();

    WatchSession *old_session = session;

    session = plcManager::instanse()->startWatch(m_ctx, param, this);
    if (session != old_session){
        connect(session, &WatchSession::watchExeComleted, this, &WatchDockWidget::receiveData);
        connect(session, &WatchSession::connected, this, [this](){
            connBtn->setToolTip(tr("Отключиться от PLC для отладки"));
            connBtn->setText(tr("Disconnect"));
            intervalSpin->setEnabled(true);
            updateTableColors();
        });
        connect(session, &WatchSession::disconnected, this, [this](){
            connBtn->setToolTip(tr("Подключиться к PLC для отладки"));
            connBtn->setText(tr("Connect"));
            intervalSpin->setEnabled(false);
            updateTableColors();
            session->deleteLater();
        });
        session->start();
    }
}

bool WatchDockWidget::isConnected() const
{
    return session && session->isConnected();
}

bool WatchDockWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == watch) {
        if (event->type() == QEvent::DragEnter) {
            // qDebug()<< "event->type() == QEvent::DragEnter";
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasText()) {
                dragEvent->acceptProposedAction();
                return true; // Перехватили, Qt дальше не передает
            }
        }
        else if (event->type() == QEvent::DragMove) {
            // qDebug()<< "event->type() == QEvent::DragMove";
            QDragMoveEvent *moveEvent = static_cast<QDragMoveEvent*>(event);
            moveEvent->acceptProposedAction();
            return true;
        }
        else if (event->type() == QEvent::Drop) {
            // qDebug()<< "event->type() == QEvent::Drop";
            QDropEvent *dropEvent = static_cast<QDropEvent*>(event);

            // Просто извлекаем текст напрямую!
            QString varName = dropEvent->mimeData()->text().trimmed();

            if (!varName.isEmpty()) {
                this->addVar(varName); // Добавляем в таблицу
            }

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            return true; // Полностью блокируем стандартный drop таблицы/модели
        }
    }
    return QDockWidget::eventFilter(watched, event);
}

void WatchDockWidget::receiveData(const QStringList &data)
{
    debugApp()<<tr("data receive from:")<< m_ctx.name << data;
    for (int i = 0; i < data.size(); ++i) {
        if (i < watchModel->rowCount()) {
            QStandardItem *valueItem = watchModel->item(i, 1);
            if (valueItem) {
                valueItem->setText(data.at(i));
            }
        }
    }
}

QStringList WatchDockWidget::collectVariables() const
{
    QStringList param;
    for (int row = 0; row < watchModel->rowCount(); ++row) {
        QStandardItem *item = watchModel->item(row, 0);
        if (item && !item->text().isEmpty()) {
            param.append(item->text());
        }
    }
    return param;
}

void WatchDockWidget::updateSessionVariables()
{
    if (session && session->isConnected()) {
        QStringList param = collectVariables();
        session->setQuery(collectVariables());
        debugApp() << tr("WatchDockWidget: Список переменных опроса динамически обновлен:") << param;
    }
}

void WatchDockWidget::showContextMenu(const QPoint &pos)
{
    QModelIndex index = watch->indexAt(pos);
    if (!index.isValid()) {
        return; // Кликнули по пустому месту таблицы, меню не показываем
    }
    int row = index.row();

    QStandardItem *varItem = watchModel->item(row, 0);
    if (!varItem || varItem->text().isEmpty()) {
        return; // Строка пустая, меню не показываем
    }
    QString varName = varItem->text();


    QMenu menu(this);

    QAction *unforce = menu.addAction(tr("Unforce"));
    unforce->setEnabled(session && session->isConnected());

    connect(unforce, &QAction::triggered, this, [this,varName, row](){
        if (session){
            QStringList get = {"get"};
            get.append(collectVariables());

            QStringList unforce = {"unforce", varName};
            session->setQuery({get, unforce});
            debugApp() << tr("WatchDockWidget: Сброс FORCE для:") << varName;
            forcedVar.remove(varName);
            updateTableColors();

            QStandardItem *fItem = watchModel->item(row, 3);
            if (fItem) {
                fItem->setText("");
            }
        }
    });

    QModelIndex varNameIndex = watchModel->index(row, 0);
    bool isUnsigned = varNameIndex.data(IsUnsignedRole).toBool();
    DisplayFormat currentFmt = static_cast<DisplayFormat>(varNameIndex.data(DisplayFormatRole).toInt());

    menu.addSeparator();
    QAction *actDefault = menu.addAction(tr("Без преобразования"));
    QAction *actDec     = menu.addAction(tr("DEC (Десятичный)"));
    QAction *actHex     = menu.addAction(tr("HEX (Шестнадцатеричный)"));
    QAction *actBin     = menu.addAction(tr("Binary (Битовый ряд)"));

    actDefault->setCheckable(true);
    actDec->setCheckable(true);
    actHex->setCheckable(true);
    actBin->setCheckable(true);

    actDefault->setChecked(currentFmt == DisplayFormat::Default);
    actDec->setChecked(currentFmt == DisplayFormat::Dec);
    actHex->setChecked(currentFmt == DisplayFormat::Hex);
    actBin->setChecked(currentFmt == DisplayFormat::Binary);

    auto trySetFormat = [this, varNameIndex, row](DisplayFormat newFmt, const QString &formatName) {
        QStandardItem *valueItem = watchModel->item(row, 1);
        QString rawValue = valueItem ? valueItem->text().trimmed() : "";

        bool canConvert = false;
        if (newFmt == DisplayFormat::Default || rawValue.isEmpty()) {
            canConvert = true;
        } else {
            rawValue.toLongLong(&canConvert);
        }

        if (canConvert) {
            watchModel->setData(varNameIndex, static_cast<int>(newFmt), DisplayFormatRole);
            watch->viewport()->update();
        } else {
            QMessageBox::warning(
                this,
                tr("Ошибка смены формата"),
                tr("Не удалось переключить режим в %1.\nТекущее значение \"%2\" не является целым числом.")
                    .arg(formatName, rawValue)
                );
            watch->viewport()->update();
        }
    };

    connect(actDefault, &QAction::triggered, this, [trySetFormat]() {
        trySetFormat(DisplayFormat::Default, "Default");
    });
    connect(actDec, &QAction::triggered, this, [trySetFormat]() {
        trySetFormat(DisplayFormat::Dec, "DEC");
    });
    connect(actHex, &QAction::triggered, this, [trySetFormat]() {
        trySetFormat(DisplayFormat::Hex, "HEX");
    });
    connect(actBin, &QAction::triggered, this, [trySetFormat]() {
        trySetFormat(DisplayFormat::Binary, "Binary");
    });

    menu.addSeparator();

    QAction *actSigned   = menu.addAction(tr("Знаковое (Signed)"));
    QAction *actUnsigned = menu.addAction(tr("Беззнаковое (Unsigned)"));

    actSigned->setCheckable(true);
    actUnsigned->setCheckable(true);

    actSigned->setChecked(!isUnsigned);
    actUnsigned->setChecked(isUnsigned);

    if (currentFmt == DisplayFormat::Default) {
        actSigned->setEnabled(false);
        actUnsigned->setEnabled(false);
    } else {
        actSigned->setEnabled(true);
        actUnsigned->setEnabled(true);
    }

    connect(actSigned, &QAction::triggered, this, [this, varNameIndex]() {
        watchModel->setData(varNameIndex, false, IsUnsignedRole);
        watch->viewport()->update();
    });
    connect(actUnsigned, &QAction::triggered, this, [this, varNameIndex]() {
        watchModel->setData(varNameIndex, true, IsUnsignedRole);
        watch->viewport()->update();
    });

    menu.exec(watch->viewport()->mapToGlobal(pos));
}

void WatchDockWidget::updateTableColors()
{
    bool connected = (session && session->isConnected());
    debugApp()<< "QSet<QString> forcedVar is :" << forcedVar;
    for (int row = 0; row < watchModel->rowCount(); ++row) {
        // Получаем имя переменной из первой колонки
        QStandardItem *varItem = watchModel->item(row, 0);
        if (!varItem || varItem->text().isEmpty()) continue;

        QString varName = varItem->text();

        // По умолчанию — стандартный цвет (прозрачный/системный)
        QBrush rowColor = QBrush();

        if (connected) {
            // Если имя переменной есть в вашем QSet forcedVar — красим в красный
            if (forcedVar.contains(varName)) {
                rowColor = QBrush(QColor(255, 200, 200)); // Мягкий красный
            } else {
                rowColor = QBrush(QColor(220, 245, 220)); // Мягкий зеленый (успешный опрос)
            }
        }

        // Применяем цвет фона ко 2 колонке в текущей строке
        QStandardItem *cellItem = watchModel->item(row, 1);
        if (cellItem) {
            cellItem->setData(rowColor, Qt::BackgroundRole);
        }
    }
}

void WatchDockWidget::addVar(const QString &varName)
{
    QStandardItem *varNameItem   = new QStandardItem();
    QStandardItem *valueItem     = new QStandardItem();
    QStandardItem *setValueItem  = new QStandardItem();
    QStandardItem *forceValueItem = new QStandardItem();
    valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    watchModel->appendRow({varNameItem, valueItem, setValueItem, forceValueItem});
    if (varName.isEmpty())
        return;
    varNameItem->setText(varName);
}
