#include "configwidget.h"
#include <QVBoxLayout>

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout* layout = new QVBoxLayout(this);
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
    emit modifiedChanged(false);
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
        emit modifiedChanged(modified);
    }
}
