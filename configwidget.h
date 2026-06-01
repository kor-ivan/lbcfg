#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include <QTextEdit>

class ConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigWidget(QWidget *parent = nullptr);
    void setConfig(const QString& yaml);
    QString config() const;
    bool isModified() const;

signals:
    void modifiedChanged(bool modified);
private slots:
    void onTextChanged();


private:
    QTextEdit* editor = nullptr;
    QString originalYaml;
    bool modified = false;
};

#endif // CONFIGWIDGET_H
