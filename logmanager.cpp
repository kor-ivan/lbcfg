#include "logmanager.h"
#include <QRegularExpression>

LogManager::LogManager(const plcManager::CommandContext &ctx, bool parse, Source source)
    : m_ctx(ctx), m_parse(parse), m_source(source), m_level(LogCatcher::Debug)
{}

LogManager::LogManager(Source source, Level level, Wrapped wrap)
: m_source(source), m_level(level), m_wrap(wrap)
{}

LogManager::LogManager(const plcManager::CommandContext &ctx, Source source, Level level, Wrapped wrap)
    : m_ctx(ctx), m_source(source), m_level(level),
    m_wrap(wrap), m_timestamp(QDateTime::currentDateTime())
{}

// LogManager::LogManager(const QDateTime &timestamp, Source source, Level level, Wrapped wrap)
//     : m_source(source), m_level(level), m_wrap(wrap),
//     m_timestamp(timestamp.isValid() ? timestamp : QDateTime::currentDateTime())
// {}

LogManager::~LogManager()
{
    if (m_level <= loglevel){
        if (m_parse){
            // Регулярное выражение с альтернативными группами:
            // Вариант 1 (Группы 1, 2, 3): ^\s*([IWE])\s*\(([^)]+)\)\s*(.*)$  --> I (12345) message
            // Вариант 2 (Группы 4, 5):    ^\s*([0-9]+)\s+(.*)$               --> 12345 message
            static const QRegularExpression logRegex(
                R"(^\s*(?:([IWE])\s*\(([^)]+)\)\s*(.*)|([0-9]+)\s+(.*))$)",
                QRegularExpression::DotMatchesEverythingOption
                );
            QRegularExpressionMatch match = logRegex.match(m_buffer);

            if (match.hasMatch()) {
                QString timeContent;
                if (!match.captured(4).isEmpty()){
                    timeContent = match.captured(4);  // Миллисекунды
                    bool ok;
                    qint64 us = timeContent.toLongLong(&ok);
                    if (ok) {
                        m_timestamp = QDateTime::fromMSecsSinceEpoch(us/1000, Qt::UTC);
                        m_timetype = LogCatcher::TimeUptime;
                    }
                    m_buffer = match.captured(5);  // Оставшийся текст сообщения
                }else{
                    QString levelLetter = match.captured(1);
                    if (levelLetter == "I")      m_level = LogCatcher::Info;
                    else if (levelLetter == "W") m_level = LogCatcher::Warning;
                    else if (levelLetter == "E") m_level = LogCatcher::Alarm;

                    timeContent = match.captured(2);
                    if (timeContent.contains(':')) {
                        QTime parsedTime = QTime::fromString(timeContent, "HH:mm:ss.zzz");
                        if (parsedTime.isValid()) {
                            m_timestamp = QDateTime(QDate::currentDate(), parsedTime);
                        }
                        m_timetype = LogCatcher::TimeReal;
                    }else{
                        bool ok;
                        qint64 ms = timeContent.toLongLong(&ok);
                        if (ok) {
                            m_timestamp = QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC);
                            m_timetype = LogCatcher::TimeUptime;
                        }
                    }
                    m_buffer = match.captured(3).trimmed();
                }
                m_buffer.remove('\r');
                if (m_buffer.contains('{') || m_buffer.contains('\n')) {
                    m_wrap = LogCatcher::wrapYes;
                }
            }
        }
        emit catcher()->newLogEntry(m_timestamp, m_source, m_level, m_buffer, m_ctx, m_wrap, m_timetype);
    }
}
LogCatcher *LogManager::catcher()
{
    static LogCatcher instanse;
    return &instanse;
}

void LogManager::setLoglevel(const Level &newLoglevel)
{
    loglevel = newLoglevel;
}
