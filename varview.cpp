#include "varview.h"
#include "lbyaml.h"
#include "logmanager.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QDrag>
#include <QMimeData>
#include <QPainter>


#include <QStyledItemDelegate>
#include <QApplication>
#include <QMouseEvent>
class CenteredCheckBoxDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        if (index.data(Qt::CheckStateRole).isValid()) {
            opt.features |= QStyleOptionViewItem::HasCheckIndicator;
            QStyle* style = opt.widget ? opt.widget->style() : qApp->style();
            QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);
            int dx = opt.rect.left() + (opt.rect.width() - checkRect.width()) / 2 - checkRect.left();
            int dy = opt.rect.top() + (opt.rect.height() - checkRect.height()) / 2 - checkRect.top();
            opt.rect.translate(dx, dy);
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Проверяем, что у элемента есть галочка, и по ней кликнули (или нажали пробел)
        if (index.data(Qt::CheckStateRole).isValid() &&
            (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick))
        {
            QStyle* style = opt.widget ? opt.widget->style() : qApp->style();
            QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);

            // Считаем геометрию смещенного в центр квадратика точно так же, как в методе paint()
            int dx = opt.rect.left() + (opt.rect.width() - checkRect.width()) / 2 - checkRect.left();
            int dy = opt.rect.top() + (opt.rect.height() - checkRect.height()) / 2 - checkRect.top();
            checkRect.translate(dx, dy);

            // Проверяем, попал ли курсор мыши внутрь нашей отцентрированной галочки
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && checkRect.contains(mouseEvent->pos())) {
                // Меняем состояние галочки на противоположное
                Qt::CheckState currentState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
                Qt::CheckState newState = (currentState == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
                return model->setData(index, newState, Qt::CheckStateRole);
            }
        }

        // Если это не клик по галочке, передаем событие стандартному обработчику
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

varView::varView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *varLayout = new QVBoxLayout(this);
    varLayout->setContentsMargins(0, 0, 0, 0);
    // 1. Создаем таблицу
    varTableView = new QTableView(this);

    // 2. Настраиваем внешний вид таблицы (современный стиль)
    varTableView->setShowGrid(true); // Включаем сетку
    varTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять строку целиком
    varTableView->setSelectionMode(QAbstractItemView::SingleSelection); // Выделять только одну строку за раз
    varTableView->setAlternatingRowColors(true); // Чередование цветов строк для читаемости

    CenteredCheckBoxDelegate* checkDelegate = new CenteredCheckBoxDelegate(this);
    varTableView->setItemDelegateForColumn(3, checkDelegate); // Центрируем Multisource
    varTableView->setItemDelegateForColumn(4, checkDelegate); // Центрируем Retain
    varTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(varTableView, &QTableView::customContextMenuRequested,
            this, &varView::showContextMenu);
    // Разрешаем перетаскивание из таблицы
    // varTableView->setDragEnabled(true);
    // varTableView->setDragDropMode(QAbstractItemView::DragOnly);
    varTableView->viewport()->installEventFilter(this);


    // Настройка шрифта (опционально, можно сделать моноширинным, как редактор)
    // varTableView->setFont(QFont("Courier New", 10));

    // 3. Настраиваем поведение заголовков
    QHeaderView *horHeader = varTableView->horizontalHeader();
    // horHeader->setSectionResizeMode(QHeaderView::Stretch); // Колонки растягиваются на всю доступную ширину
    horHeader->setSectionResizeMode(QHeaderView::Interactive);
    horHeader->setStretchLastSection(true); // Последний столбец займет всё оставшееся место
    horHeader->setVisible(true); // Показываем верхние заголовки

    QHeaderView *vertHeader = varTableView->verticalHeader();
    vertHeader->setDefaultSectionSize(22); // Компактная высота строк
    vertHeader->setVisible(true);

    // 4. Создаем модель данных и привязываем её к таблице
    varModel = new QStandardItemModel(this);
    varTableView->setModel(varModel);
    QStringList headers = {"Name", "Var", "Var_out", "Multi", "Retain", "Init"};

    varModel->setHorizontalHeaderLabels(headers);
    varLayout->addWidget(varTableView);
}

void varView::updateData(lbyaml *parser)
{
    if (!parser) return;
    if (!varModel) return;
    varModel->removeRows(0, varModel->rowCount());

    lbyaml::lbvarstat statstr = parser->getVarStat();
    logPLC(parser->getlbhost(), LogCatcher::Info, LogCatcher::wrapYes)<<
        "Number of variables :"<<statstr.quantity<<Qt::endl<<
        "Must Multisource :" << (statstr.mustMultisource.empty() ? "none" : statstr.mustMultisource.join(", "))<<Qt::endl<<
        "Handling Var :" << (statstr.handlingVar.empty() ? "none" : statstr.handlingVar.join(", "))<<Qt::endl<<
        "not added Forte :" << (statstr.noaddedForte.empty() ? "none" : statstr.noaddedForte.join(", "))<<Qt::endl<<
        "Error :" << parser->getErr();
    m_lbVarMap = parser->getLbVarMap();


    disconnect(varModel, &QStandardItemModel::dataChanged,
            this, &varView::onDataChanged);

    int j = 0;
    for (auto i = m_lbVarMap.begin(); i != m_lbVarMap.end(); ++i){
        const auto &v = i.value();
        // 0. Колонка Name (Только для чтения)
        QStandardItem* nameItem = new QStandardItem(i.key());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        varModel->setItem(j, 0, nameItem);

        // 1. Колонка Var (Только для чтения)
        QStandardItem* varItem = new QStandardItem(decompose(v.var));
        varItem->setFlags(varItem->flags() & ~Qt::ItemIsEditable);
        varModel->setItem(j, 1, varItem);

        // 2. Колонка Var_out (Только для чтения)
        QStandardItem* varOutItem = new QStandardItem(decompose(v.var_out));
        varOutItem->setFlags(varOutItem->flags() & ~Qt::ItemIsEditable);
        varModel->setItem(j, 2, varOutItem);

        // 3. Колонка Multisource (Галочка + запрет текстового ввода)
        QStandardItem* multiItem = new QStandardItem(); // Текст оставляем пустым
        multiItem->setFlags((multiItem->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        multiItem->setData(v.multisource ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        varModel->setItem(j, 3, multiItem);

        // 4. Колонка Retain (Галочка + запрет текстового ввода)
        QStandardItem* retainItem = new QStandardItem();
        retainItem->setFlags((retainItem->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        retainItem->setData(v.retain ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        varModel->setItem(j, 4, retainItem);

        varModel->setItem(j, 5, new QStandardItem(v.init));
        ++j;
    }
    // varTableView->resizeColumnsToContents();
    varTableView->setColumnWidth(1, 150);
    varTableView->setColumnWidth(2, 150);
    varTableView->setColumnWidth(3, 50); // Ширина для "Multisource"
    varTableView->setColumnWidth(4, 50); // Ширина для "Retain"

    modified = false;
    connect(varModel, &QStandardItemModel::dataChanged,
            this, &varView::onDataChanged);
}

QMap<QString, lbyaml::lbvar> varView::getUpdatedData()
{
    for (int row = 0; row < varModel->rowCount(); ++row) {
        QString varName = varModel->item(row, 0)->text();
        if (!m_lbVarMap.contains(varName)) continue;

        lbyaml::lbvar &v = m_lbVarMap[varName];

        if (QStandardItem* multiItem = varModel->item(row, 3)) {
            v.multisource = (multiItem->data(Qt::CheckStateRole).toInt() == Qt::Checked);
        }
        if (QStandardItem* retainItem = varModel->item(row, 4)) {
            v.retain = (retainItem->data(Qt::CheckStateRole).toInt() == Qt::Checked);
        }
        if (QStandardItem* initItem = varModel->item(row, 5)) {
            v.init = initItem->text();
        }
    }
    return m_lbVarMap;
}

QString varView::decompose(const QList<QStringList> &data)
{
    QString str;
    foreach (auto k, data)
        str += QString("(%1)").arg(k.join(","));
    return str;
}

void varView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    modified = true;
    emit onChanged();
}

void varView::showContextMenu(const QPoint &pos)
{
    QModelIndex index = varTableView->indexAt(pos);
    if (!index.isValid()) return;
    QString varName = varModel->item(index.row(), 0)->text();
    if (varName.isEmpty()) return;

    QMenu menu(this);
    QAction *addToWatchAction = menu.addAction("Добавить в Watch");

    connect(addToWatchAction, &QAction::triggered, this, [this, varName]() {
        emit addVariableToWatch(varName);
    });

    menu.exec(varTableView->viewport()->mapToGlobal(pos));
}

bool varView::isModified() const
{
    return modified;
}

bool varView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == varTableView->viewport()) {
        // 1. Запоминаем позицию, когда пользователь нажал левую кнопку мыши
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // qDebug()<< "event->type() == QEvent::MouseButtonPress & mouseEvent->button() == Qt::LeftButton";
                m_dragStartPos = mouseEvent->pos();
            }
        }
        // 2. Отслеживаем движение мыши с зажатой кнопкой
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // qDebug()<< "event->type() == QEvent::MouseMove & mouseEvent->buttons() & Qt::LeftButton";
                // Проверяем, что мышь сместилась достаточно далеко, чтобы это не было случайным кликом
                if ((mouseEvent->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    // qDebug()<< "(mouseEvent->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()";
                    // Определяем, над какой ячейкой сейчас находится курсор
                    QModelIndex index = varTableView->indexAt(m_dragStartPos);
                    if (index.isValid()) {
                        QString varName = varModel->item(index.row(), 0)->text();
                        if (!varName.isEmpty()) {
                            // Создаем объект Drag вручную
                            QDrag *drag = new QDrag(this);
                            QMimeData *mimeData = new QMimeData;
                            // Записываем СТРОГО чистый текст
                            mimeData->setText(varName);
                            drag->setMimeData(mimeData);

                            // Переключаем индекс на колонку 0 (столбец Name) для правильного снимка ячейки
                            QModelIndex nameIndex = index.siblingAtColumn(0);

                            // 1. Получаем прямоугольник (координаты и размеры) ячейки в varTableView
                            QRect cellRect = varTableView->visualRect(nameIndex);
                            // 2. Создаем пустую картинку (Pixmap) точно по размеру ячейки
                            QPixmap pixmap(cellRect.size());
                            // 3. Заставляем viewport таблицы отрисовать (отрендерить) область этой ячейки в нашу картинку
                            varTableView->viewport()->render(&pixmap, QPoint(0, 0), cellRect);
                            // 4. Делаем картинку полупрозрачной
                            QPixmap transparentPixmap(pixmap.size());
                            transparentPixmap.fill(Qt::transparent);
                            QPainter painter(&transparentPixmap);
                            painter.setOpacity(0.7); // Уровень прозрачности (0.0 — невидимый, 1.0 — плотный)
                            painter.drawPixmap(0, 0, pixmap);
                            painter.end();
                            // 5. Устанавливаем картинку в объект drag
                            drag->setPixmap(transparentPixmap);
                            // 6. Смещаем точку привязки картинки к курсору мыши,
                            // QPoint hotSpot = m_dragStartPos - cellRect.topLeft();
                            QPoint hotSpot;
                            if (cellRect.contains(m_dragStartPos)) {
                                // Пользователь тащит прямо за имя — оставляем естественную привязку "строго под мышью"
                                hotSpot = m_dragStartPos - cellRect.topLeft();
                            } else {
                                // Пользователь тащит за другой столбец — центрируем картинку Name ровно под курсором
                                hotSpot = QPoint(transparentPixmap.width() / 2, transparentPixmap.height() / 2);
                            }
                            drag->setHotSpot(hotSpot);
                            // Запускаем перетаскивание (программа "замрет" на этой строке, пока drag не завершится)
                            drag->exec(Qt::CopyAction);
                            return true; // Событие обработано, предотвращаем стандартное выделение строк в таблице
                        }
                    }
                }
            }
        }

    }
    return QWidget::eventFilter(watched, event);
}
