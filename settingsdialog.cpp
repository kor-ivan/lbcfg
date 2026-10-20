#include "settingsdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QDir>
#include <QCheckBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QMessageBox>
#include "appsettings.h"
#include "mainwindow.h"
#include "commandmanager.h"
#include "logmanager.h"
#include "logdockwidget.h"


SettingsDialog::SettingsDialog(QWidget *parent)
    :QDialog(parent), p_mainWindow(qobject_cast<MainWindow*>(parent))
{
    setupUi();
    setModal(true);
}

void SettingsDialog::onCategoryChanged(int index)
{
    if (pagesContainer && index >= 0 && index < pagesContainer->count()) {
        pagesContainer->setCurrentIndex(index);
    }
}

void SettingsDialog::saveSettings()
{
    if (strategyCombo)
        updateSettings(strategyCombo,
                       strategyCombo->currentData().toInt(),
                       AppSettings::getFirmwareStrtegy(),
                       AppSettings::setFirmwareStrtegy);

    if (ignoreCpuModuleCheck) {
        updateSettings(ignoreCpuModuleCheck,
                       ignoreCpuModuleCheck->isChecked(),
                       AppSettings::getFirmwareIgnoreCPU(),
                       AppSettings::setFirmwareIgnoreCPU);
    }

    if(flowLevelCombo){
        updateSettings(flowLevelCombo,
                       flowLevelCombo->currentData().toInt(),
                       AppSettings::getMaxFlowLevel(),
                       AppSettings::setMaxFlowLevel);
    }

    if(styleYamlCombo){
        updateSettings(styleYamlCombo,
                       styleYamlCombo->currentData().toInt(),
                       AppSettings::getStyleYamlArray(),
                       AppSettings::setStyleYamlArray);
    }

    if(autoIndentCheck){
        updateSettings(autoIndentCheck,
                       autoIndentCheck->isChecked(),
                       AppSettings::getAutoIndentYaml(),
                       AppSettings::setAutoIndentYaml);
    }

    if (colorRootKeysCombo) {
        updateSettings(colorRootKeysCombo, colorRootKeysCombo->currentData().toInt(),
                       AppSettings::getColorRootKeys(), AppSettings::setColorRootKeys);
    }
    if (colorInnerKeysCombo) {
        updateSettings(colorInnerKeysCombo, colorInnerKeysCombo->currentData().toInt(),
                       AppSettings::getColorInnerKeys(), AppSettings::setColorInnerKeys);
    }
    if (colorPunctuationCombo) {
        updateSettings(colorPunctuationCombo, colorPunctuationCombo->currentData().toInt(),
                       AppSettings::getColorPunctuation(), AppSettings::setColorPunctuation);
    }
    if (colorValuesCombo) {
        updateSettings(colorValuesCombo, colorValuesCombo->currentData().toInt(),
                       AppSettings::getColorValues(), AppSettings::setColorValues);
    }
    if (colorListsCombo) {
        updateSettings(colorListsCombo, colorListsCombo->currentData().toInt(),
                       AppSettings::getColorLists(), AppSettings::setColorLists);
    }
    if (colorStringsCombo) {
        updateSettings(colorStringsCombo, colorStringsCombo->currentData().toInt(),
                       AppSettings::getColorStrings(), AppSettings::setColorStrings);
    }
    if (colorCommentsCombo) {
        updateSettings(colorCommentsCombo, colorCommentsCombo->currentData().toInt(),
                       AppSettings::getColorComments(), AppSettings::setColorComments);
    }
    if (appLogLevelCombo) {
        updateSettings(appLogLevelCombo,
                       appLogLevelCombo->currentData().toInt(),
                       AppSettings::getAppMinLogLevel(),
                       [](int newValue) {
                           AppSettings::setAppMinLogLevel(newValue);
                           LogManager::setAppMinLevel(static_cast<LogManager::Level>(newValue));
                       });
    }
    if (plcLogLevelCombo) {
        updateSettings(plcLogLevelCombo,
                       plcLogLevelCombo->currentData().toInt(),
                       AppSettings::getPlcMinLogLevel(),
                       [](int newValue) {
                           AppSettings::setPlcMinLogLevel(newValue);
                           LogManager::setPlcMinLevel(static_cast<LogManager::Level>(newValue));
                       });
    }
    if (maxLogBlockSpin) {
        updateSettings(maxLogBlockSpin,
                       maxLogBlockSpin->value(),
                       AppSettings::getMaxLogBlockCount(),
                       [this](int newValue) {
                           AppSettings::setMaxLogBlockCount(newValue);
                           if (p_mainWindow && p_mainWindow->getLogDock())
                               p_mainWindow->getLogDock()->setLogMaximumBlockCount(newValue);
                       });
    }

    if (languageCombo) {
        updateSettings(languageCombo,
                       languageCombo->currentData().toString(),
                       AppSettings::getAppLanguage(),
                       [this](const QString &newLang){
                           AppSettings::setAppLanguage(newLang);
                           QMessageBox::information(this, tr("Смена языка"),
                                                    tr("Язык интерфейса изменен. Пожалуйста, перезапустите приложение для применения изменений."));
                       });
    }
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Настройки"));
    resize(800, 500);
    setMinimumSize(700, 400);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);

    sidebarMenu = new QListWidget(this);
    sidebarMenu->setFixedWidth(170);
    sidebarMenu->setFrameShape(QFrame::StyledPanel);

    sidebarMenu->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #c0c0c0;"
        "   background-color: #ffffff;"
        "   outline: 0;"
        "   padding: 2px;"
        "}"
        "QListWidget::item {"
        "   padding: 8px 6px;"
        "   border-radius: 2px;"
        "   color: #333333;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0056b3;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "   background-color: #f0f0f0;"
        "}"
        );

    sidebarMenu->addItem(new QListWidgetItem(
        style()->standardIcon(QStyle::SP_ArrowUp), tr("Прошивка")));
    sidebarMenu->addItem(new QListWidgetItem(
        style()->standardIcon(QStyle::SP_FileIcon), tr("YAML")));
    sidebarMenu->addItem(new QListWidgetItem(
        style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Логи")));
    sidebarMenu->addItem(new QListWidgetItem(
        style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("Язык")));

    contentLayout->addWidget(sidebarMenu);
    pagesContainer = new QStackedWidget(this);
    pagesContainer->setFrameShape(QFrame::NoFrame);

    pagesContainer->addWidget(createFirmwareSettings());
    pagesContainer->addWidget(createYAMLSettings());
    pagesContainer->addWidget(createLogSettings());
    pagesContainer->addWidget(createLanguageSettings());

    contentLayout->addWidget(pagesContainer, 1);
    mainLayout->addLayout(contentLayout, 1);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this
        );
    QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    if (cancelButton) {
        cancelButton->setDefault(true);
        cancelButton->setFocus();
    }
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("ОК"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Применить"));
    mainLayout->addWidget(buttonBox);

    connect(sidebarMenu, &QListWidget::currentRowChanged, this, &SettingsDialog::onCategoryChanged);
    connect(buttonBox, &QDialogButtonBox::clicked, this, [this, buttonBox](QAbstractButton *button) {
        QDialogButtonBox::StandardButton stdBtn = buttonBox->standardButton(button);
        if (stdBtn == QDialogButtonBox::Ok) {
            saveSettings();
            accept();
        }
        else if (stdBtn == QDialogButtonBox::Cancel) {
            reject();
        }
        else if (stdBtn == QDialogButtonBox::Apply) {
            saveSettings();
        }
    });
    sidebarMenu->setCurrentRow(0);
}

QWidget *SettingsDialog::createFirmwareSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки прошивки устройств</h3>"), page));

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 5, 0, 5);
    formLayout->setSpacing(8);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    auto *btnOpenRepoDialog = new QPushButton(tr("Репозиторий прошивок..."), page);
    btnOpenRepoDialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QPushButton *resetRepoBtn = nullptr;
    QLabel *repoStatusLabel = nullptr;
    auto repoRow = createSettingsLine(btnOpenRepoDialog, &resetRepoBtn, &repoStatusLabel);
    updateRepoStatusLabel(repoStatusLabel);
    formLayout->addRow(tr("Репозиторий:"), repoRow);
    connect(btnOpenRepoDialog, &QPushButton::clicked, this, [this, repoStatusLabel]() {
        p_mainWindow->editFirmwareRepositorySettings();

    });
    connect(CommandManager::instance()->getFirmwareAnalyzer(),
            &firmwareAnalyzer::updated,
            this,[this, repoStatusLabel](){
                updateRepoStatusLabel(repoStatusLabel);
    });
    connect(resetRepoBtn, &QPushButton::clicked, this, [this](){
        AppSettings::clearFirmwareRepositoryRoot();
        p_mainWindow->editFirmwareRepositorySettings(true);
    });

    strategyCombo = new QComboBox(page);
    strategyCombo->setMinimumWidth(150);
    strategyCombo->addItem(tr("Только XZ (.bin.xz)"),  0);
    strategyCombo->addItem(tr("Только BIN (.bin)"),    1);
    strategyCombo->addItem(tr("Сначала XZ"),           2);
    strategyCombo->addItem(tr("Сначала BIN"),          3);
    strategyCombo->setCurrentIndex(AppSettings::getFirmwareStrtegy());
    QPushButton *resetDefaultStrategy = nullptr;
    auto strategyRow = createSettingsLine(strategyCombo, &resetDefaultStrategy);
    formLayout->addRow(tr("Стратегия:"), strategyRow);
    connect(resetDefaultStrategy, &QPushButton::clicked, this, [this](){
        AppSettings::clearFirmwareStrtegy();
        strategyCombo->setCurrentIndex(AppSettings::getFirmwareStrtegy());
    });

    ignoreCpuModuleCheck = new QCheckBox(tr("Игнорировать процессорный модуль при групповой прошивке"), page);
    ignoreCpuModuleCheck->setChecked(AppSettings::getFirmwareIgnoreCPU());
    QPushButton *resetDefaultCpuIgnore = nullptr;
    auto ignoreCpuModuleRow = createSettingsLine(ignoreCpuModuleCheck, &resetDefaultCpuIgnore);
    formLayout->addRow(QString(), ignoreCpuModuleRow);
    connect(resetDefaultCpuIgnore, &QPushButton::clicked, this, [this](){
        AppSettings::clearFirmwareIgnoreCPU();
        ignoreCpuModuleCheck->setChecked(AppSettings::getFirmwareIgnoreCPU());
    });

    layout->addLayout(formLayout);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createYAMLSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки представления YAML</h3>"), page));

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 5, 0, 5);
    formLayout->setSpacing(8);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    flowLevelCombo = new QComboBox(page);
    flowLevelCombo->setMinimumWidth(150);
    for (int i = 1; i <= 5; ++i)
        flowLevelCombo->addItem(QString::number(i), i);
    flowLevelCombo->setCurrentIndex(flowLevelCombo->findData(AppSettings::getMaxFlowLevel()));
    QPushButton *resetFlowLevelButton = nullptr;
    auto resetFlowLevelRow = createSettingsLine(flowLevelCombo, &resetFlowLevelButton);
    formLayout->addRow(tr("Уровень компактного представления"), resetFlowLevelRow);
    connect(resetFlowLevelButton, &QPushButton::clicked, this, [this](){
        AppSettings::clearMaxFlowLevel();
        flowLevelCombo->setCurrentIndex(flowLevelCombo->findData(AppSettings::getMaxFlowLevel()));
    });

    styleYamlCombo = new QComboBox(page);
    styleYamlCombo->setMinimumWidth(150);
    styleYamlCombo->addItem(tr("Блочный"),   1);
    styleYamlCombo->addItem(tr("Компактный"),2);
    styleYamlCombo->setCurrentIndex(styleYamlCombo->findData(AppSettings::getStyleYamlArray()));
    QPushButton *resetStyleYamlButton = nullptr;
    auto resetStyleYamlRow = createSettingsLine(styleYamlCombo, &resetStyleYamlButton);
    formLayout->addRow(tr("Тип представления списков"), resetStyleYamlRow);
    connect(resetStyleYamlButton, &QPushButton::clicked, this, [this](){
        AppSettings::clearStyleYamlArray();
        styleYamlCombo->setCurrentIndex(styleYamlCombo->findData(AppSettings::getStyleYamlArray()));
    });

    autoIndentCheck = new QCheckBox(tr("Авто отступ по Enter"), page);
    autoIndentCheck->setChecked(AppSettings::getAutoIndentYaml());
    QPushButton *resetAutoIndentCheck = nullptr;
    auto autoIndentCheckRow = createSettingsLine(autoIndentCheck, &resetAutoIndentCheck);
    formLayout->addRow(QString(), autoIndentCheckRow);
    connect(resetAutoIndentCheck, &QPushButton::clicked, this, [this](){
        AppSettings::clearAutoIndentYaml();
        autoIndentCheck->setChecked(AppSettings::getAutoIndentYaml());
    });

    auto *colorGroupBox = new QGroupBox(tr("Цвета подсветки синтаксиса"), page);
    // Внутри GroupBox создаем свой собственный FormLayout для выравнивания
    auto *groupFormLayout = new QFormLayout(colorGroupBox);
    groupFormLayout->setContentsMargins(10, 15, 10, 10); // Внутренние отступы от рамки
    groupFormLayout->setSpacing(8);
    groupFormLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    QList<ColorItem> colors = {
        { tr("Черный"), Qt::black },
        { tr("Белый"), Qt::white },
        { tr("Темно-серый"), Qt::darkGray },
        { tr("Серый"), Qt::gray },
        { tr("Светло-серый"), Qt::lightGray },
        { tr("Красный"), Qt::red },
        { tr("Темно-красный"), Qt::darkRed },
        { tr("Зеленый"), Qt::green },
        { tr("Темно-зеленый"), Qt::darkGreen },
        { tr("Синий"), Qt::blue },
        { tr("Темно-синий"), Qt::darkBlue },
        { tr("Голубой/Бирюзовый"), Qt::cyan },
        { tr("Темно-бирюзовый"), Qt::darkCyan },
        { tr("Пурпурный"), Qt::magenta },
        { tr("Темно-пурпурный"), Qt::darkMagenta },
        { tr("Желтый"), Qt::yellow },
        { tr("Темно-желтый (Горчичный)"), Qt::darkYellow }
    };
    auto createColorRow = [this, page, groupFormLayout, &colors](const QString &label, QComboBox **combo, int (*getter)(), void (*clearer)()) {
        *combo = new QComboBox(page);
        (*combo)->setMinimumWidth(200);
        populateColorComboBox(*combo, getter(), colors);
        QPushButton *resetBtn = nullptr;
        groupFormLayout->addRow(label, createSettingsLine(*combo, &resetBtn));
        connect(resetBtn, &QPushButton::clicked, this, [this, combo, getter, clearer](){
            clearer();
            int idx = (*combo)->findData(getter());
            if (idx != -1) (*combo)->setCurrentIndex(idx);
        });
    };
    createColorRow(tr("Корневые ключи:"), &colorRootKeysCombo, AppSettings::getColorRootKeys, AppSettings::clearColorRootKeys);
    createColorRow(tr("Внутренние ключи:"), &colorInnerKeysCombo, AppSettings::getColorInnerKeys, AppSettings::clearColorInnerKeys);
    createColorRow(tr("Разделители:"), &colorPunctuationCombo, AppSettings::getColorPunctuation, AppSettings::clearColorPunctuation);
    createColorRow(tr("Одиночные значения:"), &colorValuesCombo, AppSettings::getColorValues, AppSettings::clearColorValues);
    createColorRow(tr("Списки и массивы:"), &colorListsCombo, AppSettings::getColorLists, AppSettings::clearColorLists);
    createColorRow(tr("Строки в кавычках:"), &colorStringsCombo, AppSettings::getColorStrings, AppSettings::clearColorStrings);
    createColorRow(tr("Комментарии:"), &colorCommentsCombo, AppSettings::getColorComments, AppSettings::clearColorComments);

    layout->addLayout(formLayout);
    layout->addWidget(colorGroupBox);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createLogSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки логирвания</h3>"), page));

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 5, 0, 5);
    formLayout->setSpacing(8);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    auto setupLogLevelCombo = [page](QComboBox **combo, int currentLevel) {
        *combo = new QComboBox(page);
        (*combo)->setMinimumWidth(150);
        (*combo)->addItem(tr("Критический (Critical)"), 0);
        (*combo)->addItem(tr("Авария (Alarm)"),       1);
        (*combo)->addItem(tr("Предупреждение (Warning)"), 2);
        (*combo)->addItem(tr("Инфо (Info)"),          3);
        (*combo)->addItem(tr("Отладка (Debug)"),       4);

        int idx = (*combo)->findData(currentLevel);
        if (idx != -1) (*combo)->setCurrentIndex(idx);
    };

    // 1. Настройка для Application
    setupLogLevelCombo(&appLogLevelCombo, AppSettings::getAppMinLogLevel());
    QPushButton *resetAppLogBtn = nullptr;
    auto appLogRow = createSettingsLine(appLogLevelCombo, &resetAppLogBtn);
    formLayout->addRow(tr("Логи Приложения (App):"), appLogRow);

    connect(resetAppLogBtn, &QPushButton::clicked, this, [this](){
        AppSettings::clearAppMinLogLevel();
        // После сброса в ini-файле вернется дефолтное значение (обычно Info = 3)
        int idx = appLogLevelCombo->findData(AppSettings::getAppMinLogLevel());
        if (idx != -1) appLogLevelCombo->setCurrentIndex(idx);
    });

    // 2. Настройка для PLC
    setupLogLevelCombo(&plcLogLevelCombo, AppSettings::getPlcMinLogLevel());
    QPushButton *resetPlcLogBtn = nullptr;
    auto plcLogRow = createSettingsLine(plcLogLevelCombo, &resetPlcLogBtn);
    formLayout->addRow(tr("Логи Контроллера (PLC):"), plcLogRow);

    connect(resetPlcLogBtn, &QPushButton::clicked, this, [this](){
        AppSettings::clearPlcMinLogLevel();
        int idx = plcLogLevelCombo->findData(AppSettings::getPlcMinLogLevel());
        if (idx != -1) plcLogLevelCombo->setCurrentIndex(idx);
    });

    // 3. Настройка максимального количества строк
    maxLogBlockSpin = new QSpinBox(page);
    maxLogBlockSpin->setMinimumWidth(150);
    maxLogBlockSpin->setRange(100, 500000);
    maxLogBlockSpin->setSingleStep(1000);
    maxLogBlockSpin->setValue(AppSettings::getMaxLogBlockCount());
    QPushButton *resetMaxLogBtn = nullptr;
    auto maxLogRow = createSettingsLine(maxLogBlockSpin, &resetMaxLogBtn);
    formLayout->addRow(tr("Макс. количество строк:"), maxLogRow);

    connect(resetMaxLogBtn, &QPushButton::clicked, this, [this](){
        AppSettings::clearMaxLogBlockCount();
        maxLogBlockSpin->setValue(AppSettings::getMaxLogBlockCount());
    });

    static int lastValue = AppSettings::getMaxLogBlockCount();
    connect(maxLogBlockSpin, &QSpinBox::valueChanged, this, [this](int value){
        maxLogBlockSpin->blockSignals(true);

        if (lastValue == 1000){
            if (value > 1000){
                maxLogBlockSpin->setSingleStep(1000);
                maxLogBlockSpin->setValue(2000);
            }else{
                maxLogBlockSpin->setSingleStep(100);
                maxLogBlockSpin->setValue(900);
            }
        }
        lastValue = maxLogBlockSpin->value();
        maxLogBlockSpin->blockSignals(false);
    });


    layout->addLayout(formLayout);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createLanguageSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки языка</h3>"), page));

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 5, 0, 5);
    formLayout->setSpacing(8);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    languageCombo = new QComboBox(page);
    languageCombo->setMinimumWidth(200);

    QString sysLangCode = QLocale::system().name().left(2);
    QString sysLangText = (sysLangCode == QStringLiteral("ru")) ? tr("Русский") : tr("English");

    languageCombo->addItem(tr("По умолчанию (%1)").arg(sysLangText), QStringLiteral("system"));
    languageCombo->addItem(tr("Русский (Russian)"), QStringLiteral("ru"));
    languageCombo->addItem(tr("English"), QStringLiteral("en"));
#ifdef QT_DEBUG
    languageCombo->addItem(tr("Оригинальный текст (только Debug)"), QStringLiteral("none"));
#endif

    QString currentLang = AppSettings::getAppLanguage();
    int targetIdx = languageCombo->findData(currentLang);
    if (targetIdx != -1) {
        languageCombo->setCurrentIndex(targetIdx);
    }

    QPushButton *resetLangBtn = nullptr;
    auto langRow = createSettingsLine(languageCombo, &resetLangBtn);
    formLayout->addRow(tr("Язык интерфейса:"), langRow);

    connect(resetLangBtn, &QPushButton::clicked, this, [this](){
        AppSettings::clearAppLanguage();
        int idx = languageCombo->findData(AppSettings::getAppLanguage());
        if (idx != -1) languageCombo->setCurrentIndex(idx);
    });

    layout->addLayout(formLayout);

    auto *warningLabel = new QLabel(tr("<i>* Изменения вступят в силу после перезапуска приложения.</i>"), page);
    warningLabel->setStyleSheet("color: #6c757d;");
    layout->addWidget(warningLabel);

    layout->addStretch();
    return page;
}

void SettingsDialog::updateRepoStatusLabel(QLabel *label)
{
    if (!label) return;

    QString currentPath = AppSettings::firmwareRepositoryRoot();
    if (currentPath.isEmpty()) {
        label->setText(tr("<i>Репозиторий не настроен</i>"));
        label->setToolTip(QString());
    } else {
        label->setText(tr("Текущий путь: %1").arg(QDir::toNativeSeparators(currentPath)));
    }
}

#include <QPixmap>
#include <QPainter>

void SettingsDialog::populateColorComboBox(QComboBox *combo, int currentSelectedColor, const QList<ColorItem> &colors)
{
    if (!combo) return;

    for (const auto &item : colors) {
        // Создаем маленькую квадратную иконку 16x16
        QPixmap pixmap(16, 16);
        pixmap.fill(item.color);

        // Отрисовка черной рамки
        QPainter painter(&pixmap);
        painter.setPen(Qt::black);
        painter.drawRect(0, 0, 15, 15);
        painter.end();

        combo->addItem(QIcon(pixmap), item.name, static_cast<int>(item.color));
    }

    // Выставляем текущий сохраненный цвет
    int targetIndex = combo->findData(currentSelectedColor);
    if (targetIndex != -1) {
        combo->setCurrentIndex(targetIndex);
    }
}


QWidget *SettingsDialog::createSettingsLine(QWidget *mainWidget, QPushButton **outResetButton, QLabel **outStatusLabel)
{
    auto *container = new QWidget(this);
    auto *hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(8);

    hLayout->addWidget(mainWidget);

    if (outResetButton) {
        auto *resetBtn = new QPushButton(tr("Сбросить..."), container);
        resetBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        hLayout->addWidget(resetBtn);
        *outResetButton = resetBtn;
    }

    if (outStatusLabel) {
        auto *statusLabel = new QLabel(container);
        hLayout->addWidget(statusLabel);
        *outStatusLabel = statusLabel;
    }
    hLayout->addStretch();
    return container;
}
