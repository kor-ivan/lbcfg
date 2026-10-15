#include "yamlhighlighter.h"

YamlHighlighter::YamlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightRule rule;

    // 1. КОРНЕВЫЕ КЛЮЧИ (строго в начале строки: forte:, slot-1:) — ТЕМНО-ЗЕЛЕНЫЙ
    rule.pattern = QRegularExpression(QStringLiteral("^[ \t]*[^#:\\s{}]+(?=\\s*:)"));
    rule.format.setForeground(Qt::darkGreen);
    rule.format.setFontWeight(QFont::Bold);
    rules.append(rule);

    // 2. ВНУТРЕННИЕ КЛЮЧИ ЛЮБОЙ ВЛОЖЕННОСТИ
    rule.pattern = QRegularExpression(QStringLiteral("(?<=[{,])\\s*([^#:\\s{}]+)(?=\\s*:)"));
    rule.format.setForeground(Qt::blue);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);

    // 3. РАЗДЕЛИТЕЛИ (двоеточие, фигурные и квадратные скобки, запятые)
    rule.pattern = QRegularExpression(QStringLiteral("[:{},\\[\\]]"));
    rule.format.setForeground(Qt::black);
    rule.format.setFontWeight(QFont::Bold);
    rules.append(rule);

    // 4. ОБЫЧНЫЕ ОДИНОЧНЫЕ ЗНАЧЕНИЯ (после двоеточия: MSK, y, bcbase, 502)
    rule.pattern = QRegularExpression(QStringLiteral(":\\s*([^\\[\\{#,\\s][^#,\\s{}]*)"));
    rule.format.setForeground(Qt::darkRed);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);

    // 5. БЛОЧНЫЕ СПИСКИ ЧЕРЕЗ ДЕФИС (в начале строки: - EM, - cmd)
    rule.pattern = QRegularExpression(QStringLiteral("^[ \t]*-\\s+([^#\\s]+)"));
    rule.format.setForeground(Qt::magenta);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);

    // 6. ЭЛЕМЕНТЫ ИНЛАЙНОВЫХ СПИСКОВ (внутри квадратных скобок [ ... ])
    rule.pattern = QRegularExpression(QStringLiteral("\\b[\\w\\.]+(?=[^\\[\\]]*\\])"));
    rule.format.setForeground(Qt::magenta);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);

    // 7. СТРОКОВЫЕ ЗНАЧЕНИЯ В КАВЫЧКАХ
    rule.pattern = QRegularExpression(QStringLiteral("\".*?\"|'.*?'"));
    rule.format.setForeground(Qt::darkCyan);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);

    // 8. КОММЕНТАРИИ (начинаются с #)
    rule.pattern = QRegularExpression(QStringLiteral("#.*"));
    rule.format.setForeground(Qt::gray);
    rule.format.setFontItalic(true);
    rule.format.setFontWeight(QFont::Normal);
    rules.append(rule);
}

void YamlHighlighter::highlightBlock(const QString &text)
{
    for (int i = 0; i < rules.count(); ++i) {
        const HighlightRule &rule = rules.at(i);

        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();

            int startPos = match.capturedStart();
            int length = match.capturedLength();

            // Смещаем позицию закрашивания на внутреннюю группу (1) для правил, содержащих служебные символы:
            if (i == 1) {
                // Смещение для внутренних ключей (пропускаем пробелы после запятой/скобки)
                startPos = match.capturedStart(1);
                length = match.capturedLength(1);
            }
            else if (i == 3 && text.at(startPos) == ':') {
                // Смещение для обычных значений (пропускаем двоеточие)
                startPos = match.capturedStart(1);
                length = match.capturedLength(1);
            }
            else if (i == 4) {
                // Смещение для блочных списков (пропускаем дефис и пробелы)
                startPos = match.capturedStart(1);
                length = match.capturedLength(1);
            }

            if (startPos >= 0 && length > 0) {
                setFormat(startPos, length, rule.format);
            }
        }
    }
}
