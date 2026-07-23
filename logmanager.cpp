#include "logmanager.h"

LogManager::LogManager(Source source, Level level)
    : m_source(source), m_level(level), m_timestamp(QDateTime::currentDateTime())
{}

LogManager::LogManager(const QDateTime &timestamp, Source source, Level level)
    : m_source(source), m_level(level),
    m_timestamp(timestamp.isValid() ? timestamp : QDateTime::currentDateTime())
{}

LogManager::~LogManager()
{
    if (m_level <= loglevel)
        emit catcher()->newLogEntry(m_timestamp, m_source, m_level, m_buffer);
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
