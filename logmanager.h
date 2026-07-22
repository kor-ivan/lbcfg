#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QDateTime>

class LogCatcher : public QObject{
    Q_OBJECT
public:
    enum Source{
        App,
        PLC
    };
    Q_ENUM(Source);
    enum Level{
        Debug,
        Info,
        Warning,
        Alarm,
        Critical
    };
    Q_ENUM(Level);
signals:
    void newLogEntry(const QDateTime &timestamp,
                     LogCatcher::Source source,
                     LogCatcher::Level level,
                     const QString &message);
};

class LogManager : public QObject
{
    Q_OBJECT
public:

    using Source = LogCatcher::Source;
    using Level = LogCatcher::Level;

    LogManager(Source source, Level level = Level::Info);
    LogManager(const QDateTime &timestamp, Source source, Level level = Level::Info);
    ~LogManager();

    template <typename T>
    LogManager& operator<<(const T& value) {
        QDebug(&m_buffer) << value;
        return *this;
    }

    static LogCatcher* catcher();

private:
    Source m_source;
    Level m_level;
    QDateTime m_timestamp;
    QString m_buffer;
};

inline LogManager logApp(LogCatcher::Level level = LogCatcher::Info) {
    return LogManager(LogManager::Source::App, level);
}

inline LogManager logPLC(LogCatcher::Level level = LogCatcher::Info) {
    return LogManager(LogCatcher::PLC, level);
}

inline LogManager logPLC(const QDateTime &timestamp, LogCatcher::Level level = LogCatcher::Info) {
    return LogManager(timestamp, LogCatcher::PLC, level);
}

#endif // LOGMANAGER_H
