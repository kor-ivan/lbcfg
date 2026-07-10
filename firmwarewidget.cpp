#include "firmwarewidget.h"
#include <QHBoxLayout>
#include <QPalette>

FirmwareWidget::FirmwareWidget(QWidget *parent)
    : QWidget{parent}
{
    // 1. Создаем компактный горизонтальный слой без отступов
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4); // небольшое расстояние между кнопкой и баром

    const int elementHeight = 14; // Фиксированная высота для статус-бара

    // 2. Инициализация и настройка QProgressBar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFormat("Прошивка: %p%");
    progressBar->setMaximumWidth(200);
    progressBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    progressBar->setFixedHeight(elementHeight);
    // Настройка палитры (гарантируем черный цвет текста поверх ползунка)
    QPalette p = progressBar->palette();
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Text, Qt::black);
    p.setColor(QPalette::WindowText, Qt::black);
    progressBar->setPalette(p);

    // 3. Инициализация и настройка кнопки Stop
    stopButton = new QPushButton(this);
    stopButton->setFixedSize(elementHeight, elementHeight);
    stopButton->setToolTip(tr("Остановить"));
    // Применяем ваш StyleSheet для квадратной красной кнопки
    stopButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #FF0000;" /* Чистый красный цвет */
        "   border: 1px solid #CC0000;" /* Темно-красная аккуратная рамка */
        "   border-radius: 1px;"        /* Минимальное сглаживание углов */
        "}"
        "QPushButton:hover {"
        "   background-color: #D60000;" /* Цвет при наведении курсора (становится темнее) */
        "}"
        "QPushButton:pressed {"
        "   background-color: #A30000;" /* Цвет при клике (эффект нажатия) */
        "}"
        );

    // 4. Собираем виджет вместе
    layout->addWidget(stopButton);
    layout->addWidget(progressBar);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // 5. Перенаправляем внутренний клик кнопки во внешний сигнал нашего виджета
    connect(stopButton, &QPushButton::clicked, this, &FirmwareWidget::stopButtonPressed);
    // По умолчанию виджет скрыт, пока не начнется прошивка
    hide();
}

FirmwareWidget::~FirmwareWidget()
{}

void FirmwareWidget::showStatus()
{
    // progressBar->setValue(0);
    stopButton->setEnabled(true);
    show();
}

void FirmwareWidget::setProgress(int value)
{
    if (!isVisible()) {
        show();
    }
    progressBar->setValue(value);
}

void FirmwareWidget::resetAndHide()
{
    progressBar->setValue(0);
    stopButton->setEnabled(false);
    hide();
}
