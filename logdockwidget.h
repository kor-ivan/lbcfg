#ifndef LOGDOCKWIDGET_H
#define LOGDOCKWIDGET_H

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include "logmanager.h"

class LogDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit LogDockWidget(QWidget *parent = nullptr);
    // QSize sizeHint() const override { return QSize(QWidget::sizeHint().width(), 300); }

    void onLogStarted();
    void onLogFinished();

signals:
    void stopButtonPressed();

protected:
    // Переопределяем, чтобы динамически двигать кнопку при изменении размеров окна
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void appendLogEntry(const QDateTime &timestamp,
                        LogCatcher::Source source,
                        LogCatcher::Level level,
                        const QString &message,
                        LogCatcher::Wrapped wrap);
    QPlainTextEdit *logViewer = nullptr;
    QPushButton *stopButton;
    void updateButtonPosition();
};

#endif // LOGDOCKWIDGET_H
