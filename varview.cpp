#include "varview.h"
#include "lbyaml.h"
#include "logmanager.h"
#include <QVBoxLayout>
#include <QHeaderView>

#include <QStyledItemDelegate>
#include <QApplication>
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

void varView::updateData(const QString &yamlText, const QString &name)
{
    if (!varModel) return;
    varModel->removeRows(0, varModel->rowCount());

    lbyaml *y = new lbyaml(yamlText, lbyaml::data);
    y->setlbhost(name);
    lbyaml::lbvarstat statstr = y->getVarStat();
    logPLC(name, LogCatcher::Info, LogCatcher::wrapYes)<<
        "Number of variables :"<<statstr.quantity<<Qt::endl<<
        "Must Multisource :" << (statstr.mustMultisource.empty() ? "none" : statstr.mustMultisource.join(", "))<<Qt::endl<<
        "Handling Var :" << (statstr.handlingVar.empty() ? "none" : statstr.handlingVar.join(", "))<<Qt::endl<<
        "not added Forte :" << (statstr.noaddedForte.empty() ? "none" : statstr.noaddedForte.join(", "))<<Qt::endl<<
        "Error :" << y->getErr();
    QMap<QString, lbyaml::lbvar> lbVarMap = y->getLbVarMap();
    y->deleteLater();
    int j = 0;
    for (auto i = lbVarMap.begin(); i != lbVarMap.end(); ++i){
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
}

QString varView::decompose(const QList<QStringList> &data)
{
    QString str;
    foreach (auto k, data)
        str += QString("(%1)").arg(k.join(","));
    return str;
}
