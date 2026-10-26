#ifndef YAMLHIGHLIGHTER_H
#define YAMLHIGHLIGHTER_H

#include <QObject>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class YamlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit YamlHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightRule> rules;

};

#endif // YAMLHIGHLIGHTER_H
