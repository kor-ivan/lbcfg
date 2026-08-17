#ifndef YAMLTEXTVIEW_H
#define YAMLTEXTVIEW_H

#include <QWidget>
#include <QTextEdit>

class yamlTextView : public QWidget
{
    Q_OBJECT
public:
    explicit yamlTextView(QWidget *parent = nullptr);
    void setText(const QString& yaml);

    void scrollToLine(int lineNumber);
    QList<QAction*> textActions() const;
    QString text() const;
    void replacePlcBlock(int startLine, int endLine, const QString &newText);

signals:
    void TextChanged();

private:
    void showCustomContextMenu(const QPoint &pos);
    QTextEdit *editor = nullptr;
};

#endif // YAMLTEXTVIEW_H
