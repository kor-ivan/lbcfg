#include "logmanager.h"

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
    if (m_level <= loglevel)
        emit catcher()->newLogEntry(m_timestamp, m_source, m_level, m_buffer, m_wrap);
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
