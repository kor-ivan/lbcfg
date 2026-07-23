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
        Critical,
        Alarm,
        Warning,
        Info,
        Debug
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
        QDebug(&m_buffer).noquote() << value;
        return *this;
    }

    static LogCatcher* catcher();

    void setLoglevel(const Level &newLoglevel);

private:
    Source m_source;
    Level m_level;
    QDateTime m_timestamp;
    QString m_buffer;
    Level loglevel = LogCatcher::Debug;
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

inline LogManager debugPLC() {
    return LogManager(LogCatcher::PLC, LogCatcher::Debug);
}

inline LogManager debugApp() {
    return LogManager(LogCatcher::App, LogCatcher::Debug);
}

#endif // LOGMANAGER_H
