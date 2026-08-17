#include "yamltextview.h"
#include <QMenu>
#include <QVBoxLayout>
#include <QTextBlock>
#include "logmanager.h"
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

void yamlTextView::scrollToLine(int lineNumber)
{
    if (!editor) return;

    QTextDocument *doc = editor->document();
    QTextBlock block = doc->findBlockByLineNumber(lineNumber);

    if (block.isValid()) {
        // 1. Создаем первый курсор для самого конца документа и временно отправляем экран туда
        QTextCursor bottomCursor(doc);
        bottomCursor.movePosition(QTextCursor::End);
        editor->setTextCursor(bottomCursor);
        // 2. Создаем целевой курсор на нужной строке
        QTextCursor targetCursor(block);
        // 3. Устанавливаем его. Так как экран был в самом низу, Qt прокрутит
        // документ ровно настолько, чтобы целевая строка только-только показалась сверху!
        editor->setTextCursor(targetCursor);
        editor->ensureCursorVisible();
        editor->setFocus();
    }else {
        debugApp() << "Не удалось найти текстовый блок для строки:" << lineNumber;
    }
}

QList<QAction *> yamlTextView::textActions() const
{
    if (!editor) return QList<QAction*>();

    QMenu *tempMenu = editor->createStandardContextMenu();
    QList<QAction*> actions = tempMenu->actions();

    for (QAction *act : actions) {
        if (act) act->setParent(const_cast<yamlTextView*>(this));
    }

    tempMenu->deleteLater();
    return actions;
}

QString yamlTextView::text() const
{
    return editor ? editor->toPlainText() : QString();
}

void yamlTextView::replacePlcBlock(int startLine, int endLine, const QString &newText)
{
    if (!editor) return;

    QTextDocument *doc = editor->document();

    int startIdx = startLine - 1;
    int endIdx = (endLine > 0) ? (endLine - 1) : (doc->blockCount() - 1);

    startIdx = qMax(0, qMin(startIdx, doc->blockCount() - 1));
    endIdx = qMax(startIdx, qMin(endIdx, doc->blockCount() - 1));

    QTextBlock startBlock = doc->findBlockByLineNumber(startIdx);
    QTextBlock endBlock = doc->findBlockByLineNumber(endIdx);

    if (startBlock.isValid() && endBlock.isValid()) {
        QTextCursor cursor(doc);
        editor->blockSignals(true);
        cursor.setPosition(startBlock.position());
        cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        cursor.insertText(newText);
        editor->blockSignals(false);
        // emit TextChanged();
    }
}

