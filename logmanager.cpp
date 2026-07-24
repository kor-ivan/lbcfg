#include "logmanager.h"
#include <QRegularExpression>

LogManager::LogManager(bool parse, Source source)
    :m_parse(parse), m_source(source), m_level(LogCatcher::Debug)
{}

LogManager::LogManager(Source source, Level level, Wrapped wrap)
    : m_source(source), m_level(level),
    m_wrap(wrap), m_timestamp(QDateTime::currentDateTime())
{}

LogManager::LogManager(const QDateTime &timestamp, Source source, Level level, Wrapped wrap)
    : m_source(source), m_level(level), m_wrap(wrap),
    m_timestamp(timestamp.isValid() ? timestamp : QDateTime::currentDateTime())
{}

LogManager::~LogManager()
{
    if (m_level <= loglevel){
        if (m_parse){
            static const QRegularExpression logRegex(
                R"(^\s*([IWE])\s*\(([^)]+)\)\s*(.*)$)",
                QRegularExpression::DotMatchesEverythingOption
                );
            QRegularExpressionMatch match = logRegex.match(m_buffer);

            if (match.hasMatch()) {
                QString levelLetter = match.captured(1);
                if (levelLetter == "I")      m_level = LogCatcher::Info;
                else if (levelLetter == "W") m_level = LogCatcher::Warning;
                else if (levelLetter == "E") m_level = LogCatcher::Alarm;

                QString timeContent = match.captured(2);
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
                qDebug()<<levelLetter<<timeContent;
                if (m_buffer.contains('{') || m_buffer.contains('\n')) {
                    m_wrap = LogCatcher::wrapYes;
                }
            }
        }
        emit catcher()->newLogEntry(m_timestamp, m_source, m_level, m_buffer, m_wrap);
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
