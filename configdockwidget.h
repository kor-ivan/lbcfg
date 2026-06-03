#ifndef CONFIGDOCKWIDGET_H
#define CONFIGDOCKWIDGET_H

#include <QDockWidget>
#include <QTextEdit>

class ConfigDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit ConfigDockWidget(const QString& name, QWidget *parent = nullptr);
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

#endif // CONFIGDOCKWIDGET_H
