#include "logdockwidget.h"
#include <QMetaEnum>
#include <QVBoxLayout>
#include <QScrollBar>

LogDockWidget::LogDockWidget(QWidget *parent)
    : QDockWidget{"LogViewer", parent}
{
    setAttribute(Qt::WA_DeleteOnClose);

    QWidget *centralWidget = new QWidget(this);

    logViewer = new QPlainTextEdit(centralWidget);
    logViewer->setReadOnly(true);
    logViewer->setMinimumHeight(50); // Минумум, чтобы совсем не пропал
    logViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMinimumSize(QSize(0, 50)); // Сбрасываем ограничения самого дока

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(logViewer);
    setWidget(centralWidget);


    const int elementHeight = 14;
    stopButton = new QPushButton(logViewer);
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

    stopButton->hide();
    connect(stopButton, &QPushButton::clicked, this, &LogDockWidget::stopButtonPressed);

    logViewer->installEventFilter(this);

    connect(LogManager::catcher(), &LogCatcher::newLogEntry,
            this, &LogDockWidget::appendLogEntry);
}

void LogDockWidget::onLogStarted()
{
    stopButton->show();
    updateButtonPosition();
}

void LogDockWidget::onLogFinished()
{
    stopButton->hide();
}

bool LogDockWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == logViewer && event->type() == QEvent::Resize) {
        updateButtonPosition();
    }
    return QDockWidget::eventFilter(obj, event);
}

void LogDockWidget::appendLogEntry(const QDateTime &timestamp, LogCatcher::Source source, LogCatcher::Level level, const QString &message, LogCatcher::Wrapped wrap)
{
    QString timeStr = timestamp.toString("HH:mm:ss.zzz");
    QString sourceStr;
    QString sourceColor = "gray";
    switch (source) {
    case LogCatcher::App:
        sourceStr = "[APP]";
        sourceColor = "SteelBlue";
        break;
    case LogCatcher::PLC:
        sourceStr = "[PLC]";
        sourceColor = "darkCyan";
        break;
    default:
        sourceStr = "[UNK]";
        sourceColor = "gray";
        break;
    }

    QString levelStr = QString::fromUtf8(QMetaEnum::fromType<LogCatcher::Level>().valueToKey(static_cast<LogCatcher::Level>(level))).toUpper();
    if (levelStr.isEmpty()) levelStr = "UNK";

    QString levelColor = "black"; // Цвет по умолчанию
    switch (level) {
    case LogCatcher::Debug:
        levelColor = "blue";
        break;
    case LogCatcher::Warning:
        levelColor = "orange";
        break;
    case LogCatcher::Alarm:
    case LogCatcher::Critical:
        levelColor = "red";
        break;
    default:
        break; // Для Info и остальных останется черный цвет
    }

    // QString wrappedMessage = (wrap == LogCatcher::wrapYes)?
    //         QString("<pre style='margin: 0; font-family: monospace;'>%1</pre>")
    //                              .arg(message.toHtmlEscaped()):message.toHtmlEscaped();

    QString htmlLine = QString("<font color='gray'>%1</font> "
                               "<b><font color='%2'>%3</font> <font color='%4'>%5:</font></b> %6")
                           .arg(timeStr)
                           .arg(sourceColor)
                           .arg(sourceStr)
                           .arg(levelColor)
                           .arg(levelStr)
                           // .arg(message.toHtmlEscaped());
                           .arg((wrap == LogCatcher::wrapYes)?
                                    QString("<br><span style='white-space: pre-wrap;'>%1</span>")
                                        .arg(message.toHtmlEscaped()):message.toHtmlEscaped());
    logViewer->appendHtml(htmlLine);
    logViewer->moveCursor(QTextCursor::End);
}

void LogDockWidget::updateButtonPosition()
{
    if (!stopButton->isVisible()) return;

    int padding = 10; // Отступ от краев
    int btnWidth = stopButton->width();

    int scrollBarWidth = 0;
    if (logViewer->verticalScrollBar()->isVisible()) {
        scrollBarWidth = logViewer->verticalScrollBar()->width();
    }

    // Вычисляем x так, чтобы кнопка была у правого края с учетом отступа
    int x = logViewer->width() - btnWidth - padding - scrollBarWidth;
    int y = padding;

    stopButton->move(x, y);
}
