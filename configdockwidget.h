#ifndef CONFIGDOCKWIDGET_H
#define CONFIGDOCKWIDGET_H

#include <QDockWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableView>
#include "plcmanager.h"
#include "yamltextview.h"
#include "varview.h"
#include "deviceview.h"


class ConfigDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit ConfigDockWidget(const QString& name,
                              QWidget *parent = nullptr);

    // QString config() const;
    bool isModified() const;
    void setConfig(const QString &yaml);
    bool openFile(const QString &filePath);
    bool saveFile();
    bool saveFileAs();

    QString getCurrentFilePath() const;
    QString getPlcName() const;

    void onConfigureClicked();
    QList<QAction*> activeTextActions() const;

signals:
    // void getSaveFile();
    void updateScan(const QString& ipv6, const QString& name);

private slots:
    void onTextChanged();


    void scrollToSelectedPlc(int index);


protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    plcManager *lbplc = nullptr;

    // Добавить в private секцию в configdockwidget.h
    QListWidget *sidebarMenu = nullptr;
    QStackedWidget *stackedContainer = nullptr;
    // Страницы-контейнеры
    yamlTextView *yamlPage = nullptr;
    varView *varPage = nullptr;
    deviceView *devicePage = nullptr;

    // QTextEdit* editor = nullptr;
    // Новые таблицы для представлений
    QTableView *varTableView = nullptr;
    QTableView *deviceTableView = nullptr;

    QComboBox *plcSelector = nullptr;
    QPushButton *configButton = nullptr;
    QString originalYaml;
    bool modified = false;
    QString plcName;
    bool writeFile(const QString &filePath);

    QString currentFilePath;
    void updateTitle();
    QStandardItemModel* createPlcModel(const QString &yamlText);

    // Слот для обработки переключения страниц
    void onSidebarRowChanged(int index);
};

#endif // CONFIGDOCKWIDGET_H
