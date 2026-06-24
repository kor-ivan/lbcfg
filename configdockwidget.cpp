#include "configdockwidget.h"
#include "qmenu.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QTableView>
#include <QHeaderView>
#include <QTextBlock>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include "lbyaml.h"
#include "lbclient.h"


ConfigDockWidget::ConfigDockWidget(const QString &name, DeviceTreeDockWidget *treeDock, QWidget *parent)
    : QDockWidget(QString("Конфигурация: %1").arg(name),parent), treeDockWidget(treeDock)
{
    plcName = name;
    qDebug()<<name<<"into ConfigDockWidget";
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setSpacing(0);  // Расстояние МЕЖДУ topLayout и QTextEdit (минимальное)

    QHBoxLayout* topLayout = new QHBoxLayout();

    configButton = new QPushButton("Сконфигурировать", this);
    configButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    topLayout->addWidget(configButton,1);


    plcSelector = new QComboBox(this);
    plcSelector->setPlaceholderText("Выберите ПЛК..."); // Подсказка, если список пуст
    QTableView* tableView = new QTableView(this);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять всю строку целиком
    tableView->setSelectionMode(QAbstractItemView::SingleSelection); // Только один ПЛК за раз
    tableView->setShowGrid(false); // Отключаем сетку для более чистого вида
    tableView->verticalHeader()->hide(); // Прячем левую нумерацию строк
    // Настраиваем поведение колонок, чтобы они занимали всё доступное место
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->verticalHeader()->setDefaultSectionSize(20); // Задаем высоту строк такую же, как и у QComboBox (20px)
    tableView->horizontalHeader()->hide(); // Прячем верхние заголовки, если они не нужны
    plcSelector->setView(tableView); // Устанавливаем таблицу как выпадающий виджет
    topLayout->addWidget(plcSelector,3);
    topLayout->addStretch(3);
    layout->addLayout(topLayout);
    plcSelector->installEventFilter(this);

    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);
    connect(editor, &QTextEdit::textChanged, this, &ConfigDockWidget::onTextChanged);
    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QTextEdit::customContextMenuRequested, this, &ConfigDockWidget::showCustomContextMenu);

    connect(configButton, &QPushButton::clicked, this, &ConfigDockWidget::onConfigureClicked);

    connect(plcSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigDockWidget::scrollToSelectedPlc);
}

void ConfigDockWidget::setConfig(const QString &yaml)
{
    originalYaml = yaml;
    editor->blockSignals(true);
    editor->setPlainText(yaml);
    editor->blockSignals(false);
    modified = false;
    plcSelector->blockSignals(true);
    plcSelector->clear();

    // Используем новый метод для генерации модели
    plcSelector->setModel(createPlcModel(yaml));
    plcSelector->setModelColumn(0);
    // plcSelector->setCurrentIndex(-1); // Показываем подсказку при открытии нового файла
    plcSelector->blockSignals(false);
}

QString ConfigDockWidget::config() const
{
    return editor->toPlainText();
}

bool ConfigDockWidget::isModified() const
{
    return modified;
}

bool ConfigDockWidget::openFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    setConfig(in.readAll());
    currentFilePath = filePath;
    updateTitle();
    return true;
}

bool ConfigDockWidget::saveFile()
{
    if (currentFilePath.isEmpty())
        return saveFileAs();
    else
        return writeFile(currentFilePath);
}

bool ConfigDockWidget::saveFileAs()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Сохранить конфигурацию как...",
                                                    plcName,
                                                    "YAML Files (*.yaml *.yml);;All Files (*)");
    if (filePath.isEmpty())
        return false;

    return writeFile(filePath);
}

bool ConfigDockWidget::writeFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл");
        return false;
    }
    QTextStream out(&file);
    out << editor->toPlainText();
    currentFilePath = filePath;
    originalYaml = editor->toPlainText(); // Сбрасываем флаг модификации
    modified = false;
    plcName = QFileInfo(filePath).fileName();
    updateTitle();
    return true;
}

void ConfigDockWidget::onTextChanged()
{
    if (editor->toPlainText() != originalYaml){
        modified = true;
        setWindowTitle(QString("* Конфигурация: %1").arg(plcName));
    }else{
        modified = false;
        setWindowTitle(QString("Конфигурация: %1").arg(plcName));
    }
}

void ConfigDockWidget::showCustomContextMenu(const QPoint &pos)
{
    // Создаем стандартное меню для QTextEdit, чтобы не терять логику (Undo, Copy, Paste)
    QMenu *standardMenu = editor->createStandardContextMenu(pos);
    standardMenu->addSeparator();
    QAction *saveAction = standardMenu->addAction("Сохранить");
    connect(saveAction, &QAction::triggered, this, [this](){
        saveFile();
    });
    QAction *confAction = standardMenu->addAction("Сконфигурировать");
    connect(confAction, &QAction::triggered, this, [this](){
        onConfigureClicked();
    });
    standardMenu->exec(editor->mapToGlobal(pos));
    delete standardMenu;
}

void ConfigDockWidget::onConfigureClicked()
{
    QString currentSelected = plcSelector->currentText();
    plcSelector->blockSignals(true);
    plcSelector->clear();

    // Передаем актуальный текст из редактора в наш метод
    plcSelector->setModel(createPlcModel(editor->toPlainText()));
    plcSelector->setModelColumn(0);

    // Восстанавливаем позицию
    int index = plcSelector->findText(currentSelected);
    plcSelector->setCurrentIndex(index); // Если index == -1, Qt сам покажет placeholder

    // int currentRow = plcSelector->currentIndex();
    if (index < 0) {
        QMessageBox::warning(this,
                             "Внимание",
                             "Не выбрана конфигурация");
        return;
    }
    // QString selectedPlc = plcSelector->currentText();
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(plcSelector->model());
    QString mac;
    if (model) {
        mac = model->item(index, 1)->text();
        qDebug() << "Запуск конфигурации для:" << currentSelected << "с MAC-адресом:" << mac;
    }

    if (modified || currentFilePath.isEmpty()){
        QMessageBox msgBox(QMessageBox::Question,
                           "Внимание",
                           "Конфигурация была изменена или не сохранена.\n"
                           "Сохранить и сконфигурировать?",
                           QMessageBox::Ok | QMessageBox::Cancel,
                           this);
        // Запускаем в синхронном режиме и получаем результат
        int result = msgBox.exec();
        if (result == QMessageBox::Ok) {
            // Код для сохранения файла
            saveFile();
        } else if (result == QMessageBox::Cancel) {
            // Код для отмены действия
            return;
        }
    }
    LBclient *lbc = new LBclient(this, {"conf"});
    // lbc->setTCPaddr(lbyaml::MacToIPv6(mac), 502);
    lbc->setlbHost(currentSelected, currentFilePath);
    connect(lbc, &LBclient::ExecuteCompletedStr, this, [this]
            (const QString& lbstr, const QString& message, const QModbusDevice::Error error){
        qDebug()<<lbstr<<message;

    });
    connect(lbc, &LBclient::lbDisconnect, this, [lbc, mac, currentSelected, this]
            (const QString& lbhost, const QString& message, const QModbusDevice::Error error){
        qDebug()<<lbhost<<message;
        lbc->deleteLater();
        if (!(treeDockWidget->containsName(currentSelected)))
            emit updateScan(lbyaml::MacToIPv6(mac), currentSelected);
    });
    lbc->Execute();
}

void ConfigDockWidget::scrollToSelectedPlc(int index)
{
    if (index < 0) return;
    // Извлекаем сохраненную модель
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(plcSelector->model());
    if (!model) return;
    QVariant lineData = model->item(index, 0)->data(Qt::UserRole);
    if (!lineData.isValid()) return;
    int targetLine = lineData.toInt();

    QTextDocument *doc = editor->document();
    QTextBlock block = doc->findBlockByLineNumber(targetLine);
    if (block.isValid()) {
        // 1. Создаем первый курсор для самого конца документа и временно отправляем экран туда
        QTextCursor bottomCursor(doc);
        bottomCursor.movePosition(QTextCursor::End);
        editor->setTextCursor(bottomCursor);
        // 2. Создаем целевой курсор на нужной строке
        QTextCursor targetCursor(block);
        // 3. Устанавливаем его. Так как экран был в самом низу, Qt прокрутит
        // документ ровно настолько, чтобы целевая строка только-только показалась сверху!
        editor->setTextCursor(targetCursor);
        editor->ensureCursorVisible();
        editor->setFocus();
    } else {
        qDebug() << "Не удалось найти текстовый блок для строки:" << targetLine;
    }
}

bool ConfigDockWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == plcSelector) {
        if (event->type() == QEvent::MouseButtonPress &&
            static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            QString currentSelected = plcSelector->currentText();

            plcSelector->blockSignals(true);
            plcSelector->clear();

            // Передаем актуальный текст из редактора в наш метод
            plcSelector->setModel(createPlcModel(editor->toPlainText()));
            plcSelector->setModelColumn(0);

            // Восстанавливаем позицию
            int index = plcSelector->findText(currentSelected);
            plcSelector->setCurrentIndex(index); // Если index == -1, Qt сам покажет placeholder

            plcSelector->blockSignals(false);
        }
    }

    return QDockWidget::eventFilter(watched, event);
}


QString ConfigDockWidget::getPlcName() const
{
    return plcName;
}

void ConfigDockWidget::updateTitle()
{
    QString prefix = modified ? "* " : "";
    setWindowTitle(QString("%1Конфигурация: %2").arg(prefix, plcName));
}

QStandardItemModel *ConfigDockWidget::createPlcModel(const QString &yamlText)
{
    lbyaml *y = new lbyaml(yamlText, lbyaml::data);
    QMultiMap<QString, lbyaml::lbhost> mmap = y->getallhostline();
    delete y;

    QStandardItemModel* model = new QStandardItemModel(mmap.size(), 2, this);
    int row = 0;
    for (auto it = mmap.constBegin(); it != mmap.constEnd(); ++it) {
        // Колонка 0: Имя ПЛК
        QStandardItem* nameItem = new QStandardItem(it.key());
        nameItem->setData(it.value().line, Qt::UserRole);
        model->setItem(row, 0, nameItem);

        // Колонка 1: MAC-адрес
        QStandardItem* macItem = new QStandardItem(it.value().mac);
        macItem->setForeground(QBrush(Qt::gray)); // Сохраняем ваш серый цвет для MAC
        model->setItem(row, 1, macItem);

        row++;
    }
    return model;
}

QString ConfigDockWidget::getCurrentFilePath() const
{
    return currentFilePath;
}
