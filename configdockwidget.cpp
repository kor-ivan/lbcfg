#include "configdockwidget.h"
#include "qmenu.h"
#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>

ConfigDockWidget::ConfigDockWidget(const QString &name, QWidget *parent)
    : QDockWidget(QString("Конфигурация: %1").arg(name),parent)
{
    plcName = name;
    qDebug()<<name<<"into ConfigDockWidget";
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);
    connect(editor, &QTextEdit::textChanged, this, &ConfigDockWidget::onTextChanged);
    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QTextEdit::customContextMenuRequested, this, &ConfigDockWidget::showCustomContextMenu);
}

void ConfigDockWidget::setConfig(const QString &yaml)
{
    originalYaml = yaml;
    editor->blockSignals(true);
    editor->setPlainText(yaml);
    editor->blockSignals(false);
    modified = false;
}

QString ConfigDockWidget::config() const
{
    return editor->toPlainText();
}

bool ConfigDockWidget::isModified() const
{
    return modified;
}

bool ConfigDockWidget::openFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    setConfig(in.readAll());
    currentFilePath = filePath;
    return true;
}

bool ConfigDockWidget::saveFile()
{
    if (currentFilePath.isEmpty()) return false; // Если файла нет, MainWindow должно вызвать SaveAs
    return saveFileAs(currentFilePath);
}

bool ConfigDockWidget::saveFileAs(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << editor->toPlainText();
    currentFilePath = filePath;
    originalYaml = editor->toPlainText(); // Сбрасываем флаг модификации
    modified = false;
    plcName = QFileInfo(filePath).fileName();
    updateTitle();
    emit configSaved();
    return true;
}

void ConfigDockWidget::onTextChanged()
{
    if (editor->toPlainText() != originalYaml){
        modified = true;
        setWindowTitle(QString("* Конфигурация: %1").arg(plcName));
    }else{
        modified = false;
        setWindowTitle(QString("Конфигурация: %1").arg(plcName));
    }
}

void ConfigDockWidget::showCustomContextMenu(const QPoint &pos)
{
    // Создаем стандартное меню для QTextEdit, чтобы не терять логику (Undo, Copy, Paste)
    QMenu *standardMenu = editor->createStandardContextMenu(pos);
    standardMenu->addSeparator();
    QAction *saveAction = standardMenu->addAction("Сконфигурировать");
    connect(saveAction, &QAction::triggered, this, [this](){
        ;
    });
    standardMenu->exec(editor->mapToGlobal(pos));
    delete standardMenu;
}

QString ConfigDockWidget::getPlcName() const
{
    return plcName;
}

void ConfigDockWidget::updateTitle()
{
    QString prefix = modified ? "* " : "";
    setWindowTitle(QString("%1Конфигурация: %2").arg(prefix, plcName));
}

QString ConfigDockWidget::getCurrentFilePath() const
{
    return currentFilePath;
}
