#pragma once

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>

class PieHighlighter : public QSyntaxHighlighter {
public:
    explicit PieHighlighter(QTextDocument* parent);
protected:
    void highlightBlock(const QString& text) override;
};

class ScriptEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget* parent = nullptr);
    void setErrorLines(const QList<int>& lines);
    void clearErrors();
    void goToLine(int line);
    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    QWidget* lineNumberArea_ = nullptr;
};
