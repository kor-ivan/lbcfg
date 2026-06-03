#include "configdockwidget.h"
#include <QVBoxLayout>

ConfigDockWidget::ConfigDockWidget(const QString &name, QWidget *parent)
    : QDockWidget(QString("Конфигурация: %1").arg(name),parent)
{
    plcName = name;
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);
    connect(editor, &QTextEdit::textChanged, this, &ConfigDockWidget::onTextChanged);
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
