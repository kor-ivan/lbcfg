#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QDockWidget>
#include <QTextEdit>

class ConfigWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit ConfigWidget(const QString& name, QWidget *parent = nullptr);
    void setConfig(const QString& yaml);
    QString config() const;
    bool isModified() const;

// signals:

private slots:
    void onTextChanged();

private:
    QTextEdit* editor = nullptr;
    QString originalYaml;
    bool modified = false;
    QString plcName;
};

#endif // CONFIGWIDGET_H
