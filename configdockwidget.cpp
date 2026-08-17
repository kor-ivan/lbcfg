#include "configdockwidget.h"
#include "commandmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QTableView>
#include <QHeaderView>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include "logmanager.h"



ConfigDockWidget::ConfigDockWidget(const QString &name, QWidget *parent)
    : QDockWidget(QString("Конфигурация: %1").arg(name),parent),
    lbplc(plcManager::instanse())
{
    plcName = name;
    QWidget *content = new QWidget(this);
    setWidget(content);

    // Основной вертикальный макет виджета
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // QVBoxLayout* layout = new QVBoxLayout(content);
    // layout->setSpacing(0);  // Расстояние МЕЖДУ topLayout и QTextEdit (минимальное)

    // 1. Оставляем верхнюю панель управления
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
    mainLayout->addLayout(topLayout);
    plcSelector->installEventFilter(this);

    // 2. Создаем горизонтальный макет для бокового меню и контента
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(2);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // 3. Инициализируем боковую панель навигации (QListWidget)
    sidebarMenu = new QListWidget(this);
    sidebarMenu->setFixedWidth(110); // Фиксированная компактная ширина для текста
    sidebarMenu->setSpacing(4);

    sidebarMenu->setStyleSheet(
        "QListWidget {"
        "   border: none;"
        "   background-color: #f5f5f5;" // Легкий серый фон панели
        "   outline: 0;"
        "}"
        "QListWidget::item {"
        "   padding: 8px 6px;"
        "   border-radius: 4px;"
        "   color: #333333;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0078d7;" // Синий акцент активной вкладки
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "   background-color: #e5e5e5;"
        "}"
        );

    sidebarMenu->addItem(new QListWidgetItem("Text View"));
    sidebarMenu->addItem(new QListWidgetItem("Var View"));
    sidebarMenu->addItem(new QListWidgetItem("IO View"));

    // 4. Инициализируем стек-контейнер для страниц
    stackedContainer = new QStackedWidget(this);

    // Страница 0: Текстовый редактор YAML
    yamlPage = new yamlTextView(this);
    stackedContainer->addWidget(yamlPage); // Индекс 0 в стеке

    // Страница 1: Просмотр переменных (VarView)
    varPage = new varView(this);
    stackedContainer->addWidget(varPage); // Индекс 1 в стеке

    // Страница 2: Настройка модулей ПЛК (DeviceView - задел на будущее)
    devicePage = new deviceView(this);
    stackedContainer->addWidget(devicePage); // Индекс 2 в стеке

    // 5. Собираем макет контента
    contentLayout->addWidget(sidebarMenu);
    contentLayout->addWidget(stackedContainer, 1); // Растягиваем контентную область
    mainLayout->addLayout(contentLayout);

    // 6. Подключение сигналов и слотов переключения страниц
    connect(sidebarMenu, &QListWidget::currentRowChanged,
            this, &ConfigDockWidget::onSidebarRowChanged);

    connect(yamlPage, &yamlTextView::TextChanged, this, &ConfigDockWidget::onTextChanged);
    // editor->setContextMenuPolicy(Qt::CustomContextMenu);
    // connect(editor, &QTextEdit::customContextMenuRequested, this, &ConfigDockWidget::showCustomContextMenu);

    connect(configButton, &QPushButton::clicked, this, &ConfigDockWidget::onConfigureClicked);

    connect(plcSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigDockWidget::scrollToSelectedPlc);

    QAction *confAction = new QAction("Сконфигурировать", this);
    connect(confAction, &QAction::triggered, this, [this](){
        onConfigureClicked();
    });
    CommandManager::instance()->setConfAction(confAction);

    connect(varPage, &varView::onChanged, this, [this]{
        modified = true;
        updateTitle();
    });

    // Устанавливаем дефолтную страницу (YAML) при запуске
    sidebarMenu->setCurrentRow(0);
}

void ConfigDockWidget::setConfig(const QString &yaml)
{
    if (yamlParser)
        yamlParser->deleteLater();
    yamlParser = new lbyaml(yaml, lbyaml::data, this);

    originalYaml = yaml;
    yamlPage->setText(originalYaml);
    // editor->blockSignals(true);
    // editor->setPlainText(yaml);
    // editor->blockSignals(false);
    modified = false;
    plcSelector->blockSignals(true);
    plcSelector->clear();

    // Используем новый метод для генерации модели
    plcSelector->setModel(createPlcModel(yaml));
    plcSelector->setModelColumn(0);

    plcName = plcSelector->currentText();
    yamlParser->setlbhost(plcName);
    // plcSelector->setCurrentIndex(-1); // Показываем подсказку при открытии нового файла
    plcSelector->blockSignals(false);
}

// QString ConfigDockWidget::config() const
// {
//     return yamlPage->getEditor()->toPlainText();
// }

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
    if (yamlParser)
        yamlParser->deleteLater();
    yamlParser = new lbyaml(filePath, lbyaml::file, this);
    yamlParser->setlbhost(plcName);
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
    out << yamlPage->text();
    currentFilePath = filePath;
    originalYaml = yamlPage->text(); // Сбрасываем флаг модификации
    modified = false;
    plcName = QFileInfo(filePath).fileName();
    updateTitle();
    return true;
}

void ConfigDockWidget::onTextChanged()
{
    if (yamlPage->text() != originalYaml){
        modified = true;
        // setWindowTitle(QString("* Конфигурация: %1").arg(plcName));
    }else{
        modified = false;
        // setWindowTitle(QString("Конфигурация: %1").arg(plcName));
    }
    updateTitle();
}

// void ConfigDockWidget::showCustomContextMenu(const QPoint &pos)
// {
//     // Создаем стандартное меню для QTextEdit, чтобы не терять логику (Undo, Copy, Paste)
//     QMenu *standardMenu = editor->createStandardContextMenu(pos);
//     standardMenu->addSeparator();
//     QAction *saveAction = CommandManager::instance()->getSaveAction();
//     if (saveAction)
//         standardMenu->addAction(saveAction);
//     QAction *saveAsAction = CommandManager::instance()->getSaveAsAction();
//     if (saveAsAction)
//         standardMenu->addAction(saveAsAction);

//     QAction *confAction = CommandManager::instance()->getConfAction();
//     if (confAction)
//         standardMenu->addAction(confAction);

//     standardMenu->exec(editor->mapToGlobal(pos));
//     delete standardMenu;
// }

void ConfigDockWidget::onConfigureClicked()
{
    QString currentSelected = plcSelector->currentText();
    plcSelector->blockSignals(true);
    plcSelector->clear();

    // Передаем актуальный текст из редактора в наш метод
    plcSelector->setModel(createPlcModel(yamlPage->text()));
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
        debugApp() << "Запуск конфигурации для:" << currentSelected << "с MAC-адресом:" << mac;
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
    if (lbplc)
        lbplc->startConf(currentSelected, currentFilePath);
}

QList<QAction *> ConfigDockWidget::activeTextActions() const
{
    if (yamlPage) {
        return yamlPage->textActions();
    }
    return QList<QAction*>();
}

void ConfigDockWidget::scrollToSelectedPlc(int index)
{
    if (index < 0) return;
    // Извлекаем сохраненную модель
    switch (sidebarMenu->currentRow()) {
    case 0:{
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(plcSelector->model());
        if (!model) return;
        QVariant lineData = model->item(index, 0)->data(Qt::UserRole);
        if (!lineData.isValid()) return;
        int targetLine = lineData.toInt();
        yamlPage->scrollToLine(targetLine);
        yamlParser->setlbhost(plcSelector->currentText());
        plcName = plcSelector->currentText();
    }
    break;
    case 1:{
        yamlParser->setlbhost(plcSelector->currentText());
        plcName = plcSelector->currentText();
        varPage->updateData(yamlParser);
    }
    break;
    default:
        break;
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
            plcSelector->setModel(createPlcModel(yamlPage->text()));
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
    yamlParser->setConfig(yamlPage->text(), lbyaml::data);
    QMultiMap<QString, lbyaml::lbhost> mmap = yamlParser->getallhostline();
    // lbyaml *y = new lbyaml(yamlText, lbyaml::data);
    // QMultiMap<QString, lbyaml::lbhost> mmap = y->getallhostline();
    // y->deleteLater();

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

void ConfigDockWidget::onSidebarRowChanged(int index)
{
    if (index < 0 || !stackedContainer) return;

    // Переключаем видимый виджет в контейнере
    stackedContainer->setCurrentIndex(index);

    // Логика синхронизации данных между представлениями
    if (index == 1) {
        if (modified)
            yamlParser->setConfig(yamlPage->text(), lbyaml::data);
        if (varPage)
            varPage->updateData(yamlParser);
    }
    else if (index == 2) {
        // Пользователь перешел во вкладку "Модули I/O" (DeviceView)
        // Аналогично парсим YAML под нужды конфигуратора модулей
    }
    else if (index == 0) {
        if (varPage->isModified()){
            QMap<int, QStringList> plcLineMap;
            QMultiMap<QString, lbyaml::lbhost> mmap = yamlParser->getallhostline();

            for (auto it = mmap.constBegin(); it != mmap.constEnd(); ++it) {
                QStringList plcInfo;
                plcInfo << it.key() << it.value().mac;
                plcLineMap.insert(it.value().line, plcInfo);
            }

            QString currentMac;
            QStandardItemModel* comboModel = qobject_cast<QStandardItemModel*>(plcSelector->model());
            int comboIndex = plcSelector->currentIndex();
            if (comboModel && comboIndex >= 0) {
                currentMac = comboModel->item(comboIndex, 1)->text();
            }

            auto targetIt = plcLineMap.end();
            for (auto it = plcLineMap.begin(); it != plcLineMap.end(); ++it) {
                if (it.value().at(0) == plcName && it.value().at(1) == currentMac) {
                    targetIt = it;
                    break;
                }
            }
            if (targetIt != plcLineMap.end()) {
                int startLine = targetIt.key();
                int endLine = 0;

                auto nextIt = std::next(targetIt);
                // auto nextIt = targetIt + 1;

                if (nextIt != plcLineMap.end()) {
                    endLine = nextIt.key() - 1;
                }

                yamlParser->implementLbVarMap(varPage->getUpdatedData());

                QString plcYamlText = yamlParser->getFormattedYaml(lbyaml::retainY);

                QString customHeader = QString(
                    "# --------------------------------------------------\n"
                    "# Generated automatically by Configurator UI\n"
                    "# Powered by lbyaml & yaml-cpp libraries\n"
                    "# --------------------------------------------------\n"
                    );
                QString customFooter = QString(
                    "\n# --------------------------------------------------\n"
                    );                                           QString replacementText = customHeader + plcYamlText + customFooter;

                yamlPage->replacePlcBlock(startLine, endLine, replacementText);
                yamlParser->setConfig(yamlPage->text(), lbyaml::data);
            } else {
                QMessageBox::warning(this, "Внимание",
                                     QString("Не удалось сопоставить ПЛК %1 (MAC: %2) со строками в файле.")
                                         .arg(plcName, currentMac));
            }

        }
        scrollToSelectedPlc(plcSelector->currentIndex());
    }
}

QString ConfigDockWidget::getCurrentFilePath() const
{
    return currentFilePath;
}
