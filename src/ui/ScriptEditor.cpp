#include "ScriptEditor.h"

#include <algorithm>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>

namespace {
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(ScriptEditor* editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override { return QSize(editor_->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override { editor_->lineNumberAreaPaintEvent(event); }

private:
    ScriptEditor* editor_;
};
}

PieHighlighter::PieHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

void PieHighlighter::highlightBlock(const QString& text) {
    QTextCharFormat keyword;
    keyword.setForeground(QColor(108, 182, 255));
    QTextCharFormat fn;
    fn.setForeground(QColor(210, 168, 255));
    QTextCharFormat str;
    str.setForeground(QColor(127, 212, 145));
    QTextCharFormat comment;
    comment.setForeground(QColor(110, 118, 129));
    comment.setFontItalic(true);
    QTextCharFormat number;
    number.setForeground(QColor(255, 171, 112));

    static const QStringList keywords = {
        QStringLiteral("if"), QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("elseif"),
        QStringLiteral("end"), QStringLiteral("and"), QStringLiteral("but"), QStringLiteral("or"), QStringLiteral("not"),
        QStringLiteral("var"), QStringLiteral("wait"), QStringLiteral("begin"), QStringLiteral("true"),
        QStringLiteral("false"), QStringLiteral("endif"), QStringLiteral("in"), QStringLiteral("starting"),
        QStringLiteral("while"), QStringLiteral("for"), QStringLiteral("do"), QStringLiteral("to"),
        QStringLiteral("neither"), QStringLiteral("nor")};
    for (const auto& k : keywords) {
        QRegularExpression re(QStringLiteral("\\b%1\\b").arg(k), QRegularExpression::CaseInsensitiveOption);
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), keyword);
        }
    }
    static const QStringList fns = {
        QStringLiteral("said"), QStringLiteral("say"), QStringLiteral("press"), QStringLiteral("release"),
        QStringLiteral("type"), QStringLiteral("askedfor"), QStringLiteral("keepdown"), QStringLiteral("toggle"),
        QStringLiteral("key"), QStringLiteral("keyboard"), QStringLiteral("mouse"), QStringLiteral("cursor"),
        QStringLiteral("screen"), QStringLiteral("speech"), QStringLiteral("microphone"), QStringLiteral("debug"),
        QStringLiteral("joystick"), QStringLiteral("joy"), QStringLiteral("wiimote"), QStringLiteral("nunchuk"),
        QStringLiteral("xinput"), QStringLiteral("ppjoy"), QStringLiteral("any"), QStringLiteral("midi"),
        QStringLiteral("osc"), QStringLiteral("window"), QStringLiteral("touchpad"), QStringLiteral("classic"),
        QStringLiteral("guitar"), QStringLiteral("sixaxis")};
    for (const auto& k : fns) {
        QRegularExpression re(QStringLiteral("\\b%1\\b").arg(k), QRegularExpression::CaseInsensitiveOption);
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), fn);
        }
    }
    QRegularExpression nums(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"));
    auto nit = nums.globalMatch(text);
    while (nit.hasNext()) {
        auto m = nit.next();
        setFormat(m.capturedStart(), m.capturedLength(), number);
    }
    QRegularExpression strings(QStringLiteral("(\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*')"));
    auto sit = strings.globalMatch(text);
    while (sit.hasNext()) {
        auto m = sit.next();
        setFormat(m.capturedStart(), m.capturedLength(), str);
    }
    int cpos = text.indexOf(QStringLiteral("//"));
    if (cpos < 0) cpos = text.indexOf(QLatin1Char('%'));
    QRegularExpression rem(QStringLiteral("^\\s*rem\\b"), QRegularExpression::CaseInsensitiveOption);
    auto rm = rem.match(text);
    if (rm.hasMatch()) cpos = rm.capturedStart();
    int apostrophe = text.indexOf(QLatin1Char('\''));
    if (apostrophe >= 0) {
        int quotes = 0;
        for (QChar ch : text)
            if (ch == QLatin1Char('\'')) ++quotes;
        if (quotes == 1 && (cpos < 0 || apostrophe < cpos)) cpos = apostrophe;
    }
    if (cpos >= 0) setFormat(cpos, text.size() - cpos, comment);
}

ScriptEditor::ScriptEditor(QWidget* parent) : QPlainTextEdit(parent) {
    QFont font(QStringLiteral("Consolas"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(11);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    new PieHighlighter(document());
    lineNumberArea_ = new LineNumberArea(this);
    connect(this, &QPlainTextEdit::textChanged, this, &ScriptEditor::clearErrors);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &ScriptEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &ScriptEditor::updateLineNumberArea);
    updateLineNumberAreaWidth(0);
}

int ScriptEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int max = std::max(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void ScriptEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) lineNumberArea_->scroll(0, dy);
    else lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void ScriptEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ScriptEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), QColor(30, 33, 39));
    painter.setPen(QColor(120, 124, 132));
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.drawText(0, top, lineNumberArea_->width() - 6, fontMetrics().height(), Qt::AlignRight,
                             QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void ScriptEditor::clearErrors() {
    setExtraSelections({});
}

void ScriptEditor::goToLine(int line) {
    if (line < 1) return;
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    setTextCursor(cursor);
    centerCursor();
}

void ScriptEditor::setErrorLines(const QList<int>& lines) {
    QList<QTextEdit::ExtraSelection> extras;
    QColor bg(160, 36, 36, 110);
    QColor mark(255, 90, 90);
    for (int line : lines) {
        if (line < 1) continue;
        QTextBlock block = document()->findBlockByNumber(line - 1);
        if (!block.isValid()) continue;
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.format.setUnderlineColor(mark);
        sel.cursor = QTextCursor(block);
        sel.cursor.clearSelection();
        extras.push_back(sel);
    }
    setExtraSelections(extras);
    if (!lines.isEmpty()) goToLine(lines.front());
}
