#include "settingsdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QDir>
#include "appsettings.h"
#include "mainwindow.h"
#include "commandmanager.h"


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
    if (strategyCombo){
        int newStrategy = strategyCombo->currentData().toInt();
        int oldStrategy = AppSettings::getFirmwareStrtegy();
        if (newStrategy != oldStrategy)
            AppSettings::setFirmwareStrtegy(strategyCombo->currentData().toInt());
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

    auto *repoWidget = new QWidget(page);
    auto *repoLayout = new QHBoxLayout(repoWidget);
    repoLayout->setContentsMargins(0, 0, 0, 0);
    repoLayout->setSpacing(8);

    auto *btnOpenRepoDialog = new QPushButton(tr("Репозиторий прошивок..."), repoWidget);
    // Делаем ширину фиксированной или аккуратной по контенту
    btnOpenRepoDialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *resetDefaultRepo = new QPushButton(tr("Сбросить..."), repoWidget);

    auto *repoStatusLabel = new QLabel(repoWidget);
    updateRepoStatusLabel(repoStatusLabel);

    repoLayout->addWidget(btnOpenRepoDialog);
    repoLayout->addWidget(resetDefaultRepo);
    repoLayout->addWidget(repoStatusLabel);
    repoLayout->addStretch(); // Выталкиваем элементы влево

    formLayout->addRow(tr("Репозиторий:"), repoWidget);

    // Подключаем вызов вашего существующего диалога FirmwareRepositoryDialog
    connect(btnOpenRepoDialog, &QPushButton::clicked, this, [this, repoStatusLabel]() {
        p_mainWindow->editFirmwareRepositorySettings();

    });

    connect(CommandManager::instance()->getFirmwareAnalyzer(),
            &firmwareAnalyzer::updated,
            this,[this, repoStatusLabel](){
                updateRepoStatusLabel(repoStatusLabel);
    });

    connect(resetDefaultRepo, &QPushButton::clicked, this, [this](){
        AppSettings::clearFirmwareRepositoryRoot();
        p_mainWindow->editFirmwareRepositorySettings(true);
    });

    auto *StrategyWidget = new QWidget(page);
    auto *StrategyLayout = new QHBoxLayout(StrategyWidget);
    StrategyLayout->setContentsMargins(0, 0, 0, 0);
    StrategyLayout->setSpacing(8);

    strategyCombo = new QComboBox(StrategyWidget);
    strategyCombo->setMinimumWidth(150);

    // Наполняем комбобокс элементами, привязывая enum в качестве скрытых UserRole данных
    strategyCombo->addItem(tr("Только XZ (.bin.xz)"),  0);
    strategyCombo->addItem(tr("Только BIN (.bin)"),    1);
    strategyCombo->addItem(tr("Сначала XZ"),           2);
    strategyCombo->addItem(tr("Сначала BIN"),          3);
    strategyCombo->setCurrentIndex(AppSettings::getFirmwareStrtegy());

    auto *resetDefaultStrategy = new QPushButton(tr("Сбросить..."), StrategyWidget);

    StrategyLayout->addWidget(strategyCombo);
    StrategyLayout->addWidget(resetDefaultStrategy);
    StrategyLayout->addStretch();

    formLayout->addRow(tr("Стратегия:"), StrategyWidget);

    connect(resetDefaultStrategy, &QPushButton::clicked, this, [this](){
        AppSettings::clearFirmwareStrtegy();
        strategyCombo->setCurrentIndex(AppSettings::getFirmwareStrtegy());
    });

    // Добавляем форму в основной макет страницы
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
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createLogSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки логирвания</h3>"), page));
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createLanguageSettings()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("<h3>Настройки языка</h3>"), page));
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
