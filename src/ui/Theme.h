#pragma once

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

inline void applyDarkPieTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    const QColor bg(22, 24, 28);
    const QColor bgAlt(30, 33, 39);
    const QColor text(220, 223, 228);
    const QColor disabled(120, 124, 132);
    const QColor accent(88, 166, 255);
    const QColor base(18, 20, 24);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, bgAlt);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, bgAlt);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, bgAlt);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, QColor(12, 14, 18));
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    app.setPalette(p);
    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: #16181c; }
        QPlainTextEdit, QTextEdit {
            background: #121418;
            color: #dce0e6;
            border: 1px solid #2b3038;
            selection-background-color: #3a5a86;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 13px;
        }
        QMenuBar { background: #1e2127; color: #dce0e6; }
        QMenuBar::item:selected { background: #2b3038; }
        QMenu { background: #1e2127; color: #dce0e6; border: 1px solid #2b3038; }
        QMenu::item:selected { background: #3a5a86; }
        QToolBar, #mainButtonBar { background: #1e2127; border: none; }
        #mainButtonBar { border-bottom: 1px solid #2b3038; }
        QLineEdit {
            background: #121418;
            color: #dce0e6;
            border: 1px solid #3a414c;
            padding: 4px 8px;
            border-radius: 4px;
            selection-background-color: #3a5a86;
        }
        QCheckBox { color: #dce0e6; }
        QToolButton {
            background: #2b3038;
            color: #dce0e6;
            border: 1px solid #3a414c;
            padding: 6px 14px;
            border-radius: 4px;
        }
        QToolButton:hover { background: #353b45; }
        QToolButton:checked {
            background: #1f6e34;
            color: #ffffff;
            border: 1px solid #3dd68c;
            font-weight: 600;
        }
        QStatusBar { background: #1e2127; color: #9aa3ad; }
        QTabWidget::pane { border: 1px solid #2b3038; }
        QTabBar::tab {
            background: #1e2127;
            color: #9aa3ad;
            padding: 8px 14px;
            border: 1px solid #2b3038;
        }
        QTabBar::tab:selected { background: #121418; color: #dce0e6; }
        QSplitter::handle { background: #2b3038; }
        QPushButton {
            background: #2b3038;
            color: #dce0e6;
            border: 1px solid #3a414c;
            padding: 6px 12px;
            border-radius: 4px;
        }
        QPushButton:hover { background: #353b45; }
        QPushButton:pressed { background: #3a5a86; }
    )"));
}
