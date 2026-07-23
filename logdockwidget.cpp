#include "logdockwidget.h"
#include <QMetaEnum>

LogDockWidget::LogDockWidget(QWidget *parent)
    : QDockWidget{"LogViewer", parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    logViewer = new QPlainTextEdit(this);
    logViewer->setReadOnly(true);
    logViewer->setMinimumHeight(50); // Минумум, чтобы совсем не пропал
    logViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMinimumSize(QSize(0, 50)); // Сбрасываем ограничения самого дока
    setWidget(logViewer);
    connect(LogManager::catcher(), &LogCatcher::newLogEntry,
            this, &LogDockWidget::appendLogEntry);
}

void LogDockWidget::appendLogEntry(const QDateTime &timestamp, LogCatcher::Source source, LogCatcher::Level level, const QString &message)
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
    QString htmlLine = QString("<font color='gray'>%1</font> "
                               "<b><font color='%2'>%3</font> <font color='%4'>%5:</font></b> %6")
                           .arg(timeStr)
                           .arg(sourceColor)
                           .arg(sourceStr)
                           .arg(levelColor)
                           .arg(levelStr)
                           .arg(message.toHtmlEscaped());
    logViewer->appendHtml(htmlLine);
    logViewer->moveCursor(QTextCursor::End);
}
