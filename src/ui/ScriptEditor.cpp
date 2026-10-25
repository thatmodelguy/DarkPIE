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
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int line = blockNumber + 1;
            if (errorLineNumbers_.contains(line)) {
                painter.fillRect(0, top, lineNumberArea_->width(), fontMetrics().height(),
                                 QColor(160, 40, 40));
                painter.setPen(QColor(255, 210, 210));
            } else {
                painter.setPen(QColor(120, 124, 132));
            }
            painter.drawText(0, top, lineNumberArea_->width() - 6, fontMetrics().height(), Qt::AlignRight,
                             QString::number(line));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void ScriptEditor::clearErrors() {
    if (!errorLineNumbers_.isEmpty()) {
        errorLineNumbers_.clear();
        lineNumberArea_->update();
    }
    setExtraSelections({});
}

void ScriptEditor::goToLine(int line) { goToPosition(line, 1); }

void ScriptEditor::goToPosition(int line, int column) {
    if (line < 1) return;
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    const int col = std::max(0, column - 1);
    cursor.setPosition(block.position() + std::min(col, block.length() - 1));
    setTextCursor(cursor);
    centerCursor();
}

void ScriptEditor::setErrorLines(const QList<int>& lines) {
    QList<ScriptErrorMark> marks;
    for (int line : lines) marks.push_back({line, 1, 0});
    setErrorMarks(marks);
}

void ScriptEditor::setErrorMarks(const QList<ScriptErrorMark>& marks) {
    QList<QTextEdit::ExtraSelection> extras;
    QSet<int> lines;
    // Soft whole-line tint, then a red underline on the bad token (GlovePIE caret jump).
    QColor lineBg(140, 40, 40, 55);
    QColor tokenBg(180, 50, 50, 120);
    QColor underline(255, 110, 110);
    for (const auto& mark : marks) {
        if (mark.line < 1) continue;
        lines.insert(mark.line);
        QTextBlock block = document()->findBlockByNumber(mark.line - 1);
        if (!block.isValid()) continue;

        QTextEdit::ExtraSelection lineSel;
        lineSel.format.setBackground(lineBg);
        lineSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        lineSel.cursor = QTextCursor(block);
        lineSel.cursor.clearSelection();
        extras.push_back(lineSel);

        if (mark.length > 0 && mark.column >= 1) {
            const int start = block.position() + std::min(mark.column - 1, std::max(0, block.length() - 1));
            const int avail = block.position() + block.length() - 1 - start;
            const int len = std::max(1, std::min(mark.length, avail));
            QTextEdit::ExtraSelection tok;
            tok.format.setBackground(tokenBg);
            tok.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            tok.format.setUnderlineColor(underline);
            tok.cursor = QTextCursor(document());
            tok.cursor.setPosition(start);
            tok.cursor.setPosition(start + len, QTextCursor::KeepAnchor);
            extras.push_back(tok);
        }
    }
    errorLineNumbers_ = lines;
    lineNumberArea_->update();
    setExtraSelections(extras);
    if (!marks.isEmpty()) goToPosition(marks.front().line, marks.front().column);
}
