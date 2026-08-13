#include "varview.h"
#include "lbyaml.h"
#include "logmanager.h"
#include <QVBoxLayout>
#include <QHeaderView>


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

    // qDebug().noquote()<<parser->getFormattedYaml();

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

bool varView::isModified() const
{
    return modified;
}
