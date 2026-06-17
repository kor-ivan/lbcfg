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

    bool openFile(const QString &filePath);
    bool saveFile();
    bool saveFileAs(const QString &filePath);

    QString getCurrentFilePath() const;
    QString getPlcName() const;

signals:
    void configSaved();

private slots:
    void onTextChanged();
    void showCustomContextMenu(const QPoint &pos);

private:
    QTextEdit* editor = nullptr;
    QString originalYaml;
    bool modified = false;
    QString plcName;

    QString currentFilePath;
    void updateTitle();
};

#endif // CONFIGDOCKWIDGET_H
