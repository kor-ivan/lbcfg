#ifndef CONFIGDOCKWIDGET_H
#define CONFIGDOCKWIDGET_H

#include <QDockWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QStandardItemModel>
#include "plcmanager.h"


class ConfigDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit ConfigDockWidget(const QString& name,
                              QWidget *parent = nullptr,
                              plcManager *plc = nullptr);
    void setConfig(const QString& yaml);
    QString config() const;
    bool isModified() const;

    bool openFile(const QString &filePath);
    bool saveFile();
    bool saveFileAs();

    QString getCurrentFilePath() const;
    QString getPlcName() const;

signals:
    // void getSaveFile();
    void updateScan(const QString& ipv6, const QString& name);

private slots:
    void onTextChanged();
    void showCustomContextMenu(const QPoint &pos);
    void onConfigureClicked();
    void scrollToSelectedPlc(int index);


protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    plcManager *lbplc = nullptr;
    QTextEdit* editor = nullptr;
    QComboBox *plcSelector = nullptr;
    QPushButton *configButton = nullptr;
    QString originalYaml;
    bool modified = false;
    QString plcName;
    bool writeFile(const QString &filePath);

    QString currentFilePath;
    void updateTitle();
    QStandardItemModel* createPlcModel(const QString &yamlText);
};

#endif // CONFIGDOCKWIDGET_H
