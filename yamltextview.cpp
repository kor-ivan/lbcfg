#include "yamltextview.h"
#include <QMenu>
#include <QVBoxLayout>
#include "commandmanager.h"

yamlTextView::yamlTextView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);

    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QTextEdit::customContextMenuRequested,
            this, &yamlTextView::showCustomContextMenu);
    connect(editor, &QTextEdit::textChanged,
            this, &yamlTextView::TextChanged);
}

void yamlTextView::setText(const QString &yaml)
{
    editor->blockSignals(true);
    editor->setPlainText(yaml);
    editor->blockSignals(false);
}


void yamlTextView::showCustomContextMenu(const QPoint &pos)
{
    // Создаем стандартное меню для QTextEdit, чтобы не терять логику (Undo, Copy, Paste)
    QMenu *standardMenu = editor->createStandardContextMenu(pos);
    standardMenu->addSeparator();
    QAction *saveAction = CommandManager::instance()->getSaveAction();
    if (saveAction)
        standardMenu->addAction(saveAction);
    QAction *saveAsAction = CommandManager::instance()->getSaveAsAction();
    if (saveAsAction)
        standardMenu->addAction(saveAsAction);

    QAction *confAction = CommandManager::instance()->getConfAction();
    if (confAction)
        standardMenu->addAction(confAction);

    standardMenu->exec(editor->mapToGlobal(pos));
    delete standardMenu;
}

QTextEdit *yamlTextView::getEditor() const
{
    return editor;
}

