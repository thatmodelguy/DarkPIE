#pragma once

#include <QPlainTextEdit>
#include <QSet>
#include <QSyntaxHighlighter>

struct ScriptErrorMark {
    int line = 1;
    int column = 1;
    int length = 1;
};

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
    void setErrorMarks(const QList<ScriptErrorMark>& marks);
    void clearErrors();
    void goToLine(int line);
    void goToPosition(int line, int column);
    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    QWidget* lineNumberArea_ = nullptr;
    QSet<int> errorLineNumbers_;
};
