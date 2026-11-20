#include "rtusettingsdialog.h"
#include "appsettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QSerialPort>
#include <QMessageBox>

RtuSettingsDialog::RtuSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Параметры RS-485"));
    resize(500, 300);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    auto *btnAdd = new QPushButton(tr("+ Добавить"), this);
    auto *btnRemove = new QPushButton(tr("- Удалить"), this);
    btnAdd->setMinimumHeight(24);
    btnRemove->setMinimumHeight(24);

    toolbar->addWidget(btnAdd);
    toolbar->addWidget(btnRemove);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        tr("Скорость"),
        tr("Биты данных"),
        tr("Четность"),
        tr("Стоп-биты")
    });
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    header->setHighlightSections(false);
    header->setStyleSheet(
        "QHeaderView::section {"
        "    background-color: #f0f0f0;"
        "    font-weight: bold;"
        "    border: 1px solid #dcdcdc;"
        "    padding-left: 4px;"
        "    height: 20px;"
        "}"
        );

    // Компактный вертикальный заголовок и строки
    m_table->verticalHeader()->setVisible(true);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->verticalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #dcdcdc;"
        "    padding: 0px 4px;"
        "}"
        );


    m_table->setStyleSheet(
        "QTableWidget::item { padding: 1px 4px; }"
        "QTableWidget QComboBox { border: none; background: transparent; padding-left: 4px; }"
        "QTableWidget QComboBox::drop-down { border: none; width: 18px; }"
        );
    mainLayout->addWidget(m_table);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Сохранить"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    buttonBox->button(QDialogButtonBox::Ok)->setMinimumHeight(24);
    buttonBox->button(QDialogButtonBox::Cancel)->setMinimumHeight(24);
    mainLayout->addWidget(buttonBox);

    connect(btnAdd, &QPushButton::clicked, this, [this]() { addProfileRow(); });
    connect(btnRemove, &QPushButton::clicked, this, &RtuSettingsDialog::removeSelectedRow);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RtuSettingsDialog::saveAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const auto profiles = AppSettings::getRtuProfiles();
    for (const auto &cfg : profiles) {
        addProfileRow(cfg);
    }

}

void RtuSettingsDialog::addProfileRow(const discover::ModbusRtuSettings &cfg)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *baudCombo = new QComboBox(m_table);
    for (int baud : {9600, 19200, 38400, 57600, 115200}) {
        baudCombo->addItem(QString::number(baud), baud);
    }

    const int baudVal = (cfg.baudRate > 0) ? cfg.baudRate : 9600;
    const int baudIdx = baudCombo->findData(baudVal);
    baudCombo->setCurrentIndex(baudIdx != -1 ? baudIdx : 3);
    m_table->setCellWidget(row, 0, baudCombo);

    auto *dataBitsCombo = new QComboBox(m_table);
    dataBitsCombo->addItem("7", 7);
    dataBitsCombo->addItem("8", 8);
    const int dataVal = (cfg.dataBits == 7) ? 7 : 8;
    dataBitsCombo->setCurrentIndex(dataVal == 7 ? 0 : 1);
    m_table->setCellWidget(row, 1, dataBitsCombo);

    auto *parityCombo = new QComboBox(m_table);
    parityCombo->addItem(tr("Нет (N)"), static_cast<int>(QSerialPort::NoParity));
    parityCombo->addItem(tr("Чет (E)"), static_cast<int>(QSerialPort::EvenParity));
    parityCombo->addItem(tr("Нечет (O)"), static_cast<int>(QSerialPort::OddParity));
    const int parityIdx = parityCombo->findData(cfg.parity);
    parityCombo->setCurrentIndex(parityIdx != -1 ? parityIdx : 0);
    m_table->setCellWidget(row, 2, parityCombo);

    auto *stopBitsCombo = new QComboBox(m_table);
    stopBitsCombo->addItem("1", static_cast<int>(QSerialPort::OneStop));
    stopBitsCombo->addItem("2", static_cast<int>(QSerialPort::TwoStop));
    stopBitsCombo->setCurrentIndex(cfg.stopBits == QSerialPort::TwoStop ? 1 : 0);
    m_table->setCellWidget(row, 3, stopBitsCombo);

}

void RtuSettingsDialog::removeSelectedRow()
{
    const int row = m_table->currentRow();
    if (row >= 0 && m_table->rowCount() > 1) {
        m_table->removeRow(row);
    }
}

void RtuSettingsDialog::saveAndAccept()
{
    QList<discover::ModbusRtuSettings> profiles;
    profiles.reserve(m_table->rowCount());

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *baudCombo     = qobject_cast<QComboBox*>(m_table->cellWidget(row, 0));
        auto *dataBitsCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 1));
        auto *parityCombo   = qobject_cast<QComboBox*>(m_table->cellWidget(row, 2));
        auto *stopBitsCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 3));

        if (!baudCombo || !dataBitsCombo || !parityCombo || !stopBitsCombo) {
            continue;
        }

        discover::ModbusRtuSettings cfg;
        cfg.baudRate = baudCombo->currentData().toInt();
        cfg.dataBits = dataBitsCombo->currentData().toInt();
        cfg.parity   = parityCombo->currentData().toInt();
        cfg.stopBits = stopBitsCombo->currentData().toInt();

        // Проверка на дубликат среди уже добавленных профилей
        for (int i = 0; i < profiles.size(); ++i) {
            const auto &existing = profiles.at(i);
            if (existing.baudRate == cfg.baudRate &&
                existing.dataBits == cfg.dataBits &&
                existing.parity   == cfg.parity &&
                existing.stopBits == cfg.stopBits) {
                QMessageBox::warning(
                    this,
                    tr("Дубликат профиля"),
                    tr("Параметры в строке %1 дублируют строку %2.")
                        .arg(row + 1)
                        .arg(i + 1)
                    );
                m_table->selectRow(row);
                return;
            }
        }

        profiles.append(cfg);
    }

    if (profiles.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Необходимо указать хотя бы один профиль RS-485."));
        return;
    }

    AppSettings::setRtuProfiles(profiles);
    accept();
}
