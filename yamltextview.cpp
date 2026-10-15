#include "yamltextview.h"
#include <QMenu>
#include <QVBoxLayout>
#include <QTextBlock>
#include "logmanager.h"
#include "commandmanager.h"
#include "yamlhighlighter.h"

yamlTextView::yamlTextView(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    editor = new QTextEdit(this);
    editor->setFontFamily("Courier New");
    layout->addWidget(editor);

    highlighter = new YamlHighlighter(editor->document());

    editor->installEventFilter(this);

    toggleCommentAction = new QAction(tr("Закомментировать/Раскомментировать"), this);
    toggleCommentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    toggleCommentAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    this->addAction(toggleCommentAction);
    connect(toggleCommentAction, &QAction::triggered, this, &yamlTextView::toggleComment);

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
    if (toggleCommentAction) {
        standardMenu->addAction(toggleCommentAction);
        standardMenu->addSeparator();
    }
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

    int startIdx = startLine;
    int endIdx = (endLine > 0) ? (endLine - 1) : (doc->blockCount() - 1);

    startIdx = qMax(0, qMin(startIdx, doc->blockCount() - 1));
    endIdx = qMax(startIdx, qMin(endIdx, doc->blockCount() - 1));

    QString headerMarker = "# Generated automatically by Configurator UI";

    int checkStart = qMax(0, startIdx - 5);
    for (int i = startIdx; i >= checkStart; --i) {
        QTextBlock block = doc->findBlockByLineNumber(i);
        if (block.isValid() && block.text().contains(headerMarker)) {
            // Если нашли маркер, смотрим на строку выше него.
            // Если там линия разграничения "# ---", сдвигаем индекс удаления на неё
            if (i > 0 && doc->findBlockByLineNumber(i - 1).text().contains("# ---")) {
                startIdx = i - 1;
            } else {
                startIdx = i;
            }
            break;
        }
    }


    QTextBlock startBlock = doc->findBlockByLineNumber(startIdx);
    QTextBlock endBlock = doc->findBlockByLineNumber(endIdx);

    if (startBlock.isValid() && endBlock.isValid()) {
        QTextCursor cursor(doc);
        editor->blockSignals(true);

        QString customHeader = QString(
                                   "# --------------------------------------------------\n"
                                   "%1\n"
                                   "# Powered by %2 %3 %4 \n"
                                   "# --------------------------------------------------\n"
                                   ).arg(headerMarker,
                                        QCoreApplication::organizationName(),
                                        QCoreApplication::applicationName(),
                                        QCoreApplication::applicationVersion());
        QString customFooter = QString(
            "\n# --------------------------------------------------\n"
            );
        QString formattedText = customHeader + newText + customFooter;

        // Начало заменяемого блока.
        cursor.setPosition(startBlock.position());
        if (endIdx >= doc->blockCount() - 1) {
            cursor.movePosition(
                QTextCursor::End,
                QTextCursor::KeepAnchor
            );
        } else {
            QTextBlock blockAfterEnd =
                doc->findBlockByLineNumber(endIdx + 1);

            if (blockAfterEnd.isValid()) {
                cursor.setPosition(
                    blockAfterEnd.position(),
                    QTextCursor::KeepAnchor
                );
            } else {
                cursor.movePosition(
                    QTextCursor::End,
                    QTextCursor::KeepAnchor
                );
            }
        }

        cursor.insertText(formattedText);
        editor->blockSignals(false);
        emit isReplaceComplete();
    }
}

void yamlTextView::toggleComment()
{
    if (!editor) return;

    QTextCursor cursor = editor->textCursor();
    QTextDocument *doc = editor->document();

    // Запоминаем границы выделения, чтобы восстановить их в конце
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    // Определяем номера начальной и конечной строк (блоков)
    QTextBlock startBlock = doc->findBlock(startPos);
    QTextBlock endBlock = doc->findBlock(endPos);

    // Если курсор стоит в самом начале строки при выделении снизу вверх,
    // исключаем лишнюю пустую строку из обработки
    if (cursor.hasSelection() && endPos == endBlock.position() && startPos < endPos) {
        endBlock = endBlock.previous();
    }

    int startLine = startBlock.blockNumber();
    int endLine = endBlock.blockNumber();

    // Отыщем, нужно ли нам комментировать или раскомментировать.
    // Если ХОТЯ БЫ ОДНА строка в выделении НЕ начинается с '#' (не считая пробелов),
    // то мы будем КОММЕНТИРОВАТЬ весь блок. Иначе — раскомментировать.
    bool shouldComment = false;
    for (int i = startLine; i <= endLine; ++i) {
        QTextBlock block = doc->findBlockByLineNumber(i);
        if (block.isValid()) {
            QString trimmed = block.text().trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
                shouldComment = true;
                break;
            }
        }
    }

    // Объединяем все изменения в один EditBlock для корректного Undo/Redo (Ctrl+Z)
    cursor.beginEditBlock();
    editor->blockSignals(true); // Временно отключаем сигналы, чтобы не спамить изменениями

    for (int i = startLine; i <= endLine; ++i) {
        QTextBlock block = doc->findBlockByLineNumber(i);
        if (!block.isValid()) continue;

        QString text = block.text();
        QTextCursor lineCursor(block);

        if (shouldComment) {
            // КОММЕНТИРОВАНИЕ: Вставляем "# " в самое начало строки
            lineCursor.movePosition(QTextCursor::StartOfBlock);
            lineCursor.insertText("# ");
        } else {
            // РАСКОММЕНТИРОВАНИЕ: Ищем "# " или "#" в начале строки и удаляем их
            int indentWidth = 0;
            // Считаем отступы до знака решетки
            while (indentWidth < text.length() && (text.at(indentWidth) == ' ' || text.at(indentWidth) == '\t')) {
                indentWidth++;
            }

            if (indentWidth < text.length() && text.at(indentWidth) == '#') {
                lineCursor.movePosition(QTextCursor::StartOfBlock);
                lineCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, indentWidth);

                // Проверяем, идет ли после решетки пробел, чтобы удалить "# ", а не просто "#"
                int charsToRemove = 1;
                if (indentWidth + 1 < text.length() && text.at(indentWidth + 1) == ' ') {
                    charsToRemove = 2;
                }

                lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, charsToRemove);
                lineCursor.removeSelectedText();
            }
        }
    }

    editor->blockSignals(false);
    cursor.endEditBlock();

    // Восстанавливаем выделение текста для удобства пользователя
    // (Поскольку текст сместился из-за добавления/удаления символов, просто обновляем курсор)
    editor->setTextCursor(editor->textCursor());
    emit TextChanged(); // Оповещаем систему об изменении текста
}


bool yamlTextView::eventFilter(QObject *watched, QEvent *event)
{
    // Проверяем, что событие пришло именно от нашего текстового редактора и это нажатие клавиши
    if (watched == editor && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Перехватываем Enter (обе клавиши: на основной клавиатуре и на цифровой)
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            QTextCursor cursor = editor->textCursor();
            QTextBlock currentBlock = cursor.block();
            QString currentText = currentBlock.text();

            // 1. Считаем количество пробелов и табов в начале текущей строки
            int spacesCount = 0;
            for (const QChar &ch : currentText) {
                if (ch == ' ' || ch == '\t') {
                    spacesCount++;
                } else {
                    break; // Как только пошел текст, останавливаемся
                }
            }

            // Получаем чистую строку без пробелов в начале для анализа структуры YAML
            QString trimmedText = currentText.trimmed();

            // 2. Умное дополнение: если строка заканчивается на двоеточие ':' (это новый блок),
            // или на открывающую скобку '[', '{', то делаем дополнительный отступ на 2 пробела
            int extraSpaces = 0;
            if (!trimmedText.isEmpty()) {
                if (trimmedText.endsWith(':') ||
                    trimmedText.endsWith('[') ||
                    trimmedText.endsWith('{')) {
                    extraSpaces = 2; // Шаг отступа для вложенного YAML-блока
                }
            }

            // Включаем макрос изменений, чтобы Undo/Redo (отмена по Ctrl+Z)
            // воспринимала перенос строки и добавление пробелов как ОДНО действие
            cursor.beginEditBlock();

            cursor.insertText("\n"); // Вставляем сам перенос строки

            // Вставляем накопленные пробелы текущего уровня + пробелы новой вложенности
            QString indent(spacesCount + extraSpaces, ' ');
            if (!indent.isEmpty()) {
                cursor.insertText(indent);
            }

            cursor.endEditBlock();

            editor->setTextCursor(cursor); // Обновляем позицию курсора на экране
            return true; // Говорим Qt, что мы сами обработали Enter, стандартный обработчик вызывать не нужно
        }
    }
    return QWidget::eventFilter(watched, event);
}

