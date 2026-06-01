#include "configwidget.h"
#include <QVBoxLayout>

ConfigWidget::ConfigWidget(const QString &name, QWidget *parent)
    : QDockWidget(QString("Конфигурация: %1").arg(name),parent)
{
    plcName = name;
    QWidget *content = new QWidget(this);
    setWidget(content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);
    connect(editor, &QTextEdit::textChanged, this, &ConfigWidget::onTextChanged);
}

void ConfigWidget::setConfig(const QString &yaml)
{
    originalYaml = yaml;
    editor->blockSignals(true);
    editor->setPlainText(yaml);
    editor->blockSignals(false);
    modified = false;

}

QString ConfigWidget::config() const
{
    return editor->toPlainText();
}

bool ConfigWidget::isModified() const
{
    return modified;
}

void ConfigWidget::onTextChanged()
{
    if (editor->toPlainText() != originalYaml){
        modified = true;
        setWindowTitle(QString("* Конфигурация: %1").arg(plcName));
    }else{
        modified = false;
        setWindowTitle(QString("Конфигурация: %1").arg(plcName));
    }
}
