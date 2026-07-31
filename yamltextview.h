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

    QTextEdit *getEditor() const;

signals:
    void TextChanged();

private:
    void showCustomContextMenu(const QPoint &pos);
    QTextEdit *editor = nullptr;
};

#endif // YAMLTEXTVIEW_H
