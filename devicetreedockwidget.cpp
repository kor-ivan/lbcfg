#include "devicetreedockwidget.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <QColor>
#include "logmanager.h"
#include "commandmanager.h"
#include "appsettings.h"
#include "firmwarerepositorydialog.h"

DeviceTreeDockWidget::DeviceTreeDockWidget(QWidget *parent)
    : QDockWidget("Tree View", parent), lbplc(plcManager::instanse())
{
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout *layout = new QVBoxLayout(content);
    treeView = new QTreeView(this);
    treeModel = new QStandardItemModel(this);
    treeView->setModel(treeModel);
    treeView->setHeaderHidden(true);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested,
            this, &DeviceTreeDockWidget::showContextMenu);
    connect(treeView, &QTreeView::expanded,
            this, &DeviceTreeDockWidget::onTreeExpanded);

    m_firmwareAnalyzer = new firmwareAnalyzer(this);

    layout->addWidget(treeView);
    connect(treeView, &QTreeView::doubleClicked, this,
            [this](const QModelIndex &index){
                if (!index.isValid()) return;
                if (!index.parent().isValid()){
                    // plcManager::CommandContext ctx;
                    // ctx.ipv6 = index.data(Qt::UserRole).value<QHostAddress>();
                    // ctx.name = index.data().toString();
                    emit requestConfig(lbplc->getctx(index.data(Qt::UserRole).value<QHostAddress>(), index.data().toString()));
                }
            }
            );
    connect(lbplc, &plcManager::showMessage, this, [this](const QString &title, const QString &message){
        QMessageBox::information(this, title, message);
    });
    connect(lbplc, &plcManager::restartAllCompleted, this, [this]
            (const plcManager::CommandContext &ctx){
                QMessageBox::information(this, "Перезагрузить все",
                                         QString("Команда на перезагрузку всех модулей %1 отправлена").arg(ctx.name));
            });
    // qRegisterMetaType<QHostAddress>("QHostAddress");
}

void DeviceTreeDockWidget::updateDevice(const plcManager::CommandContext &ctx, const QMap<qsizetype, lbprocess::scaninfo> &scan)
{
    QStandardItem* plcRoot = findPlcRoot(ctx.ipv6);
    QModelIndex rootIndex;

    // Если не нашли, создаем новый корень
    if (!plcRoot) {
        plcRoot = new QStandardItem(ctx.name);
        plcRoot->setData(QVariant::fromValue(ctx.ipv6), Qt::UserRole); // Прячем ID для поиска в будущем
        QFont rootFont = plcRoot->font();
        rootFont.setBold(true);
        rootFont.setPointSize(rootFont.pointSize());
        plcRoot->setFont(rootFont);
        treeModel->appendRow(plcRoot);
        rootIndex = treeModel->index(treeModel->rowCount() - 1, 0);
    }else{
        plcRoot->removeRows(0, plcRoot->rowCount());
        plcRoot->setText(ctx.name);
        rootIndex = plcRoot->index();
    }

    // Итерируем по результатам сканирования
    for (auto it = scan.begin(); it != scan.end(); ++it) {
        const auto &info = it.value();
        // Создаем элементы для двух колонок
        QStandardItem *col1 = new QStandardItem(QString("Slot %1: %2").arg(it.key()).arg(info.devtype));
        col1->setData(it.key(), Qt::UserRole);
        col1->setData(info.devtype, ModuleTypeRole);
        col1->setData(info.version, InstalledVersionRole);
        col1->setData(true, ModuleItemRole);
        // 2. Делаем их жирными
        QFont boldFont = col1->font();
        boldFont.setBold(true);
        col1->setFont(boldFont);
        if (info.master)
            col1->setText(col1->text() + " [MASTER]");
        // Добавляем их в модель как одну строку
        plcRoot->appendRow(col1);
        // Теперь добавляем подробности ВНУТРЬ (как подветки)
        col1->appendRow(new QStandardItem("MAC: " + info.mac));

        QStandardItem *versionItem =
            new QStandardItem("Version: " + info.version);
        versionItem->setData(true, VersionInfoRole);
        col1->appendRow(versionItem);

        col1->appendRow(new QStandardItem("Serial: " + info.data.value(0)));

        if (m_firmwareLoaded)
            updateFirmwareStatus(col1);
    }
    // Раскрываем дерево
    treeView->expand(rootIndex);
}

bool DeviceTreeDockWidget::containsName(const QString &name)
{
    for (int i = 0; i < treeModel->rowCount(); ++i) {
        auto *item = treeModel->item(i);
        if (item->text() == name)
            return true;
    }
    return false;
}

void DeviceTreeDockWidget::showContextMenu(const QPoint &pos)
{
    // Получаем индекс элемента, на который кликнули
    QModelIndex index = treeView->indexAt(pos);
    if (!index.isValid()) return;
    const bool isRoot = !index.parent().isValid();
    const bool isModule = index.data(ModuleItemRole).toBool();

    // MAC / Version / Serial are informational rows,
    // not command targets.
    if (!isRoot && !isModule)
        return;

    plcManager::CommandContext ctx;
    if (isRoot) {
        ctx.name = index.data().toString();
        ctx.ipv6 = index.data(Qt::UserRole).value<QHostAddress>();
    } else {
        ctx.name = index.parent().data().toString();
        ctx.ipv6 = index.parent().data(Qt::UserRole).value<QHostAddress>();
        ctx.slot = index.data(Qt::UserRole).toInt();
    }

    QMenu menu(this);
    // --- Только для Устройства ---
    if (isRoot) {
        QAction *getConfigAction = menu.addAction(QString("Запросить конфигурацию у %1").arg(ctx.name));
        QFont font = getConfigAction->font();
        font.setBold(true);
        getConfigAction->setFont(font);
        connect(getConfigAction, &QAction::triggered, this, [this, ctx]() {
            emit requestConfig(ctx);
        });

        QAction *update = menu.addAction("Обновить");
        connect(update, &QAction::triggered, this, [this, ctx](){
            emit requestUpdate(ctx);
        });

        QAction *removeAction = menu.addAction(QString("Удалить %1").arg(ctx.name));
        connect(removeAction, &QAction::triggered, this, [this, index]() {
            treeModel->removeRow(index.row());
        });

        QAction *restartAll = menu.addAction("Перезагрузить все");
        connect(restartAll, &QAction::triggered, this,
                [this, ctx](){
                    debugApp()<<"Перезагрузить все команда отправлена";
                    lbplc->startRestartAll(ctx);
                });

        QAction *fsformat = menu.addAction("Сбросить к заводским");
        connect(fsformat, &QAction::triggered, this, [this, ctx](){
            auto reply = QMessageBox::question(this,
                                               "Подтверждение сброса",
                                               QString("Вы уверены, что хотите сбросить устройство %1 к заводским настройкам?").arg(ctx.name),
                                               QMessageBox::Yes | QMessageBox::No,
                                               QMessageBox::No); // Кнопка по умолчанию
            if (reply == QMessageBox::Yes)
                lbplc->lbc_executeCommand(ctx, {"fsformat"}, "Сброс к заводским", [ctx, this] (const QStringList& res){
                    return QString("%1 сброшено к заводским настройкам. Требуется перезагрузка.")
                        .arg(ctx.name);
                });
        });

        QAction *flashAll = menu.addAction(QString("Прошить все модули %1").arg(ctx.name));
        connect(flashAll, &QAction::triggered, this, [this, ctx](){
            emit requestFlashAll(ctx);
        });

        QAction *fboot = menu.addAction(QString("Загрузить fboot в %1").arg(ctx.name));
        connect(fboot, &QAction::triggered, this, [this, ctx](){
            emit requestFboot(ctx);
        });

        QAction *nofboot = menu.addAction(QString("Удалить fboot в %1").arg(ctx.name));
        connect(nofboot, &QAction::triggered, this, [this, ctx]() {
            lbplc->lbc_executeCommand(ctx, {"nofboot"}, "Удалить fboot", [ctx](const QStringList&) {
                return QString("Команда на удаление fboot %1 отправлена").arg(ctx.displayName());
            });
        });

        menu.addSeparator();

        QAction *repositoryAction =
            menu.addAction("Репозиторий прошивок...");

        connect(repositoryAction, &QAction::triggered,
                this, [this]() {
            editFirmwareRepository();
        });

        QAction *refreshRepositoryAction =
            menu.addAction("Обновить версии из репозитория");

        connect(refreshRepositoryAction, &QAction::triggered,
                this, [this]() {
            m_repositoryPromptDeclined = false;

            QString repositoryRoot = m_repositoryRoot;

            if (repositoryRoot.isEmpty())
                repositoryRoot =
                    AppSettings::firmwareRepositoryRoot();

            if (repositoryRoot.isEmpty()) {
                editFirmwareRepository();
                return;
            }

            if (loadFirmwareRepository(repositoryRoot, true))
                updateAllFirmwareStatuses();
        });
    }
    // --- Общие действия ---
    // QAction *getUptime = menu.addAction("Время работы");
    // connect(getUptime, &QAction::triggered, this, [this, ctx]() {
    //     lbplc->lbc_executeCommand(ctx, {"get", "sys.uptime"}, "Время работы", [ctx, this](const QStringList& res) {
    //         QString uptime = res.isEmpty() ? toBold("none") : toBold(res.at(0));
    //         return QString("Время работы %1 %2 сек").arg(ctx.displayName(), uptime);
    //     });
    // });
    menu.addSeparator();
    CommandManager::instance()->getUptimeAction(ctx, &menu);
    CommandManager::instance()->getRestartAction(ctx, &menu);
    CommandManager::instance()->getFlashAction(ctx, &menu);
    menu.addSeparator();

    // QAction *restart = menu.addAction("Перезагрузить");
    // connect(restart, &QAction::triggered, this, [this, ctx]() {
    //     lbplc->lbc_executeCommand(ctx, {"set", "sys.restart=1"}, "Перезагрузка", [ctx](const QStringList&) {
    //         return QString("Команда на перезагрузку %1 отправлена").arg(ctx.displayName());
    //     });
    // });
    // QAction *flash = menu.addAction("Загрузить прошивку ...");
    // connect(flash, &QAction::triggered, this, [this, ctx](){
    //     emit requestFlash(ctx);
    // });

    CommandManager::instance()->getLogMenu(ctx, &menu);

    // QMenu *logMenu = menu.addMenu("Запросить лог");

    // QAction *logAll = logMenu->addAction("Запросить весь лог");
    // connect(logAll, &QAction::triggered, this, [this, ctx](){
    //     lbplc->startLog(ctx, "a");
    // });

    // QAction *logLast100 = logMenu->addAction("Запросить 100 сообщений");
    // connect(logLast100, &QAction::triggered, this, [this, ctx](){
    //     lbplc->startLog(ctx, "a100");
    // });

    // QAction *logLast100f = logMenu->addAction("Запросить 100 и следовать");
    // connect(logLast100f, &QAction::triggered, this, [this, ctx](){
    //     lbplc->startLog(ctx, "a100f");
    // });

    menu.exec(treeView->viewport()->mapToGlobal(pos));

}

void DeviceTreeDockWidget::onTreeExpanded(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QStandardItem *moduleItem = treeModel->itemFromIndex(index);

    if (!moduleItem
        || !moduleItem->data(ModuleItemRole).toBool()) {
        return;
    }

    const QString moduleType =
        moduleItem->data(ModuleTypeRole).toString().trimmed();

    if (moduleType.isEmpty()
        || moduleType.compare(
               QStringLiteral("unknown"),
               Qt::CaseInsensitive) == 0) {

        if (QStandardItem *versionItem =
                versionInfoItem(moduleItem)) {

            versionItem->setBackground(QBrush());

            versionItem->setToolTip(
                QStringLiteral(
                    "Тип модуля не определён, "
                    "сравнение версии недоступно"));
        }

        return;
    }

    if (!ensureFirmwareRepository()) {
        if (QStandardItem *versionItem =
                versionInfoItem(moduleItem)) {

            versionItem->setBackground(QBrush());

            versionItem->setToolTip(
                QStringLiteral(
                    "Репозиторий прошивок не настроен"));
        }

        return;
    }

    updateFirmwareStatus(moduleItem);
}


QStandardItem *
DeviceTreeDockWidget::versionInfoItem(
    QStandardItem *moduleItem) const
{
    if (!moduleItem)
        return nullptr;

    for (int row = 0;
         row < moduleItem->rowCount();
         ++row) {

        QStandardItem *child = moduleItem->child(row);

        if (child
            && child->data(VersionInfoRole).toBool()) {
            return child;
        }
    }

    return nullptr;
}


bool DeviceTreeDockWidget::ensureFirmwareRepository()
{
    if (m_firmwareLoaded)
        return true;

    QString repositoryRoot = m_repositoryRoot;

    if (repositoryRoot.isEmpty()) {
        repositoryRoot =
            AppSettings::firmwareRepositoryRoot();
    }

    if (!repositoryRoot.isEmpty()
        && loadFirmwareRepository(
            repositoryRoot,
            false)) {
        return true;
    }

    if (m_repositoryPromptDeclined)
        return false;

    // Expansion opens the settings dialog first.
    // Explorer opens only after an explicit Browse action.
    return chooseFirmwareRepository(false);
}


bool DeviceTreeDockWidget::chooseFirmwareRepository(
    bool allowClear)
{
    QString initialPath = m_repositoryRoot;

    if (initialPath.isEmpty()) {
        initialPath =
            AppSettings::firmwareRepositoryRoot();
    }

    while (true) {
        FirmwareRepositoryDialog dialog(
            initialPath,
            allowClear,
            this);

        if (dialog.exec() != QDialog::Accepted) {
            m_repositoryPromptDeclined = true;
            return false;
        }

        const QString selected =
            dialog.repositoryPath();

        if (selected.isEmpty() && allowClear) {
            clearFirmwareRepository();
            m_repositoryPromptDeclined = false;
            return true;
        }

        if (loadFirmwareRepository(
                selected,
                true)) {

            m_repositoryPromptDeclined = false;
            return true;
        }

        initialPath = selected;
    }
}


void DeviceTreeDockWidget::editFirmwareRepository()
{
    m_repositoryPromptDeclined = false;

    if (chooseFirmwareRepository(true)
        && m_firmwareLoaded) {
        updateAllFirmwareStatuses();
    }
}


void DeviceTreeDockWidget::
reloadFirmwareRepositoryFromSettings(
    bool showErrors)
{
    m_repositoryPromptDeclined = false;
    m_firmwareLoaded = false;
    m_repositoryRoot.clear();

    clearAllFirmwareStatuses();

    const QString repositoryRoot =
        AppSettings::firmwareRepositoryRoot();

    if (repositoryRoot.isEmpty())
        return;

    if (loadFirmwareRepository(
            repositoryRoot,
            showErrors)) {

        updateAllFirmwareStatuses();
    }
}


void DeviceTreeDockWidget::clearFirmwareRepository()
{
    AppSettings::clearFirmwareRepositoryRoot();

    m_repositoryRoot.clear();
    m_firmwareLoaded = false;

    clearAllFirmwareStatuses();
}


bool DeviceTreeDockWidget::loadFirmwareRepository(
    const QString &repositoryRoot,
    bool showErrors)
{
    const QFileInfo repositoryInfo(repositoryRoot);

    if (!repositoryInfo.exists()
        || !repositoryInfo.isDir()) {

        if (showErrors) {
            QMessageBox::warning(
                this,
                "Репозиторий прошивок",
                QString(
                    "Каталог репозитория не найден:\n%1")
                    .arg(repositoryRoot));
        }

        return false;
    }

    const QString firmwarePath =
        QDir(repositoryInfo.absoluteFilePath())
            .filePath(QStringLiteral("firmware"));

    const QFileInfo firmwareInfo(firmwarePath);

    if (!firmwareInfo.exists()
        || !firmwareInfo.isDir()) {

        if (showErrors) {
            QMessageBox::warning(
                this,
                "Репозиторий прошивок",
                QString(
                    "В выбранном каталоге "
                    "не найдена папка firmware:\n%1")
                    .arg(firmwarePath));
        }

        return false;
    }

    m_firmwareAnalyzer->setPath(
        firmwareInfo.absoluteFilePath());

    m_firmwareAnalyzer->update();

    if (m_firmwareAnalyzer->error()
        != firmwareAnalyzer::ok) {

        if (showErrors) {
            QMessageBox::warning(
                this,
                "Репозиторий прошивок",
                m_firmwareAnalyzer->errorString());
        }

        return false;
    }

    const QMap<QString, firmwareAnalyzer::fwinfo>
        firmwareMap =
            m_firmwareAnalyzer->getFirmwareMap();

    if (firmwareMap.isEmpty()) {
        if (showErrors) {
            QMessageBox::warning(
                this,
                "Репозиторий прошивок",
                QString(
                    "В каталоге firmware "
                    "не найдено ни одной "
                    "распознанной прошивки:\n%1")
                    .arg(
                        firmwareInfo.absoluteFilePath()));
        }

        return false;
    }

    m_repositoryRoot =
        repositoryInfo.absoluteFilePath();

    m_firmwareLoaded = true;

    AppSettings::setFirmwareRepositoryRoot(
        m_repositoryRoot);

    const QList<firmwareAnalyzer::fwinfo> rejected =
        m_firmwareAnalyzer->getRejectedFirmware();

    for (const firmwareAnalyzer::fwinfo &info
         : rejected) {

        debugApp()
            << "Firmware rejected:"
            << info.sourcePath
            << info.err
            << info.errStr;
    }

    return true;
}


void DeviceTreeDockWidget::updateFirmwareStatus(
    QStandardItem *moduleItem)
{
    if (!moduleItem || !m_firmwareLoaded)
        return;

    QStandardItem *versionItem =
        versionInfoItem(moduleItem);

    if (!versionItem)
        return;

    versionItem->setBackground(QBrush());
    versionItem->setToolTip(QString());

    const QString moduleType =
        moduleItem
            ->data(ModuleTypeRole)
            .toString()
            .trimmed();

    const QString installedVersion =
        moduleItem
            ->data(InstalledVersionRole)
            .toString()
            .trimmed();

    const QMap<QString, firmwareAnalyzer::fwinfo>
        firmwareMap =
            m_firmwareAnalyzer->getFirmwareMap();

    const auto it =
        firmwareMap.constFind(moduleType);

    if (it == firmwareMap.constEnd()) {
        versionItem->setToolTip(
            QString(
                "Устройство: %1\n"
                "В репозитории нет "
                "распознанной прошивки для %2")
                .arg(
                    installedVersion,
                    moduleType));

        return;
    }

    const firmwareAnalyzer::fwinfo
        &repositoryFirmware = it.value();

    const QString repositoryVersion =
        repositoryFirmware.version.trimmed();

    if (repositoryVersion.isEmpty()
        || repositoryVersion.compare(
               QStringLiteral("unknown"),
               Qt::CaseInsensitive) == 0) {

        versionItem->setToolTip(
            QString(
                "Устройство: %1\n"
                "Файл: %2\n"
                "Версия прошивки в репозитории "
                "не определена")
                .arg(
                    installedVersion,
                    repositoryFirmware.sourcePath));

        return;
    }

    const bool matches =
        firmwareAnalyzer::versionsMatch(
            installedVersion,
            repositoryVersion);

    if (matches) {
        versionItem->setBackground(
            QBrush(
                QColor(
                    QStringLiteral("#C3E6CB"))));
    }

    versionItem->setToolTip(
        QString(
            "Модуль: %1\n"
            "Устройство: %2\n"
            "Для сравнения: %3\n"
            "Репозиторий: %4\n"
            "Статус: %5\n"
            "Файл: %6")
            .arg(
                moduleType,
                installedVersion,
                firmwareAnalyzer::normalizeVersion(
                    installedVersion),
                repositoryVersion,
                matches
                    ? QStringLiteral(
                          "версии совпадают")
                    : QStringLiteral(
                          "версии не совпадают"),
                repositoryFirmware.sourcePath));
}


void DeviceTreeDockWidget::clearAllFirmwareStatuses()
{
    for (int rootRow = 0;
         rootRow < treeModel->rowCount();
         ++rootRow) {

        QStandardItem *root =
            treeModel->item(rootRow);

        if (!root)
            continue;

        for (int moduleRow = 0;
             moduleRow < root->rowCount();
             ++moduleRow) {

            QStandardItem *moduleItem =
                root->child(moduleRow);

            if (!moduleItem
                || !moduleItem
                        ->data(ModuleItemRole)
                        .toBool()) {
                continue;
            }

            if (QStandardItem *versionItem =
                    versionInfoItem(moduleItem)) {

                versionItem->setBackground(
                    QBrush());

                versionItem->setToolTip(
                    QStringLiteral(
                        "Репозиторий прошивок "
                        "не настроен"));
            }
        }
    }
}


void DeviceTreeDockWidget::updateAllFirmwareStatuses()
{
    if (!m_firmwareLoaded)
        return;

    for (int rootRow = 0;
         rootRow < treeModel->rowCount();
         ++rootRow) {

        QStandardItem *root =
            treeModel->item(rootRow);

        if (!root)
            continue;

        for (int moduleRow = 0;
             moduleRow < root->rowCount();
             ++moduleRow) {

            QStandardItem *moduleItem =
                root->child(moduleRow);

            if (moduleItem
                && moduleItem
                       ->data(ModuleItemRole)
                       .toBool()) {

                updateFirmwareStatus(moduleItem);
            }
        }
    }
}

QStandardItem *DeviceTreeDockWidget::findPlcRoot(const QHostAddress& ipv6)
{
    for (int i = 0; i < treeModel->rowCount(); ++i) {
        auto *item = treeModel->item(i);
        if (item->data(Qt::UserRole).value<QHostAddress>() == ipv6)
            return item;
    }
    return nullptr;
}

// QString DeviceTreeDockWidget::toBold(const QString &text)
// {
//     return QString("<b>%1</b>").arg(text);
// }
