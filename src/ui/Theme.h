#pragma once

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

inline void applyDarkPieTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    const QColor bg(11, 17, 30);
    const QColor bgAlt(18, 28, 48);
    const QColor text(220, 230, 245);
    const QColor disabled(110, 128, 155);
    const QColor accent(64, 156, 255);
    const QColor base(10, 16, 28);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, bgAlt);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, bgAlt);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, QColor(20, 32, 54));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, QColor(8, 12, 22));
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    app.setPalette(p);
    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #0a1020, stop:0.55 #0c1528, stop:1 #0e1a32);
            color: #dce6f5;
        }
        QToolTip {
            background: #142038;
            color: #dce6f5;
            border: 1px solid #3a5f9a;
            padding: 6px 10px;
            border-radius: 8px;
        }

        QPlainTextEdit, QTextEdit {
            background: rgba(10, 16, 28, 220);
            color: #e6eef8;
            border: 1px solid #2a3f66;
            border-radius: 10px;
            selection-background-color: #2f5f9e;
            selection-color: #ffffff;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 13px;
            padding: 4px;
        }
        QPlainTextEdit:focus, QTextEdit:focus {
            border: 1px solid #4a8dff;
        }

        QMenuBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #16233c, stop:1 #101b30);
            color: #dce6f5;
            border-bottom: 1px solid #243656;
            padding: 2px 6px;
            spacing: 2px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 12px;
            border-radius: 6px;
            margin: 2px 1px;
        }
        QMenuBar::item:selected {
            background: rgba(64, 156, 255, 55);
            color: #ffffff;
        }
        QMenuBar::item:pressed {
            background: rgba(64, 156, 255, 90);
        }
        QMenu {
            background: #121e36;
            color: #dce6f5;
            border: 1px solid #355280;
            border-radius: 10px;
            padding: 6px;
        }
        QMenu::item {
            padding: 7px 28px 7px 16px;
            border-radius: 6px;
            margin: 1px 2px;
        }
        QMenu::item:selected {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #2a5fad, stop:1 #1e4a8c);
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #2a3f66;
            margin: 5px 10px;
        }
        QMenu::indicator {
            width: 14px;
            height: 14px;
            margin-left: 6px;
        }

        #mainButtonBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #15233d, stop:1 #0f1a30);
            border: none;
            border-bottom: 1px solid #2a4068;
        }
        #mainButtonBar QLabel {
            color: #9eb4d4;
            font-weight: 600;
            font-size: 12px;
        }
        #mainButtonBar #micIcon {
            color: #7cbcff;
            font-weight: 400;
            font-size: 14px;
            padding: 0 2px 0 0;
            min-width: 18px;
        }
        #mainButtonBar #micPercent {
            color: #c8d7eb;
            min-width: 34px;
        }

        #findBar {
            background: #101c34;
            border-top: 1px solid #2a4068;
            border-bottom: 1px solid #2a4068;
        }
        #findBar QLabel {
            color: #9eb4d4;
            font-weight: 600;
        }

        #debugPanel {
            background: transparent;
        }
        #debugHeader {
            color: #9eb4d4;
            font-weight: 700;
            font-size: 12px;
            letter-spacing: 0.4px;
            padding: 2px 2px 0 4px;
        }
        #debugOutput {
            background: rgba(8, 14, 26, 230);
            border: 1px solid #2a3f66;
            border-radius: 10px;
            color: #b8d0a8;
        }

        QLineEdit {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0c1424, stop:1 #101c34);
            color: #e6eef8;
            border: 1px solid #355280;
            padding: 6px 10px;
            border-radius: 8px;
            selection-background-color: #2f5f9e;
        }
        QLineEdit:focus {
            border: 1px solid #4a8dff;
            background: #0e1830;
        }

        QCheckBox {
            color: #c8d7eb;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #3a5f9a;
            background: #0e1830;
        }
        QCheckBox::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a9cff, stop:1 #2a6fd0);
            border: 1px solid #7cbcff;
        }

        QSlider::groove:horizontal {
            height: 8px;
            border-radius: 4px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #1a2a48, stop:1 #1e3858);
            border: 1px solid #2a4068;
        }
        QSlider::sub-page:horizontal {
            border-radius: 4px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #2a7dff, stop:1 #3dd68c);
        }
        QSlider::add-page:horizontal {
            border-radius: 4px;
            background: transparent;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e8f2ff, stop:1 #8ec0ff);
            border: 1px solid #4a8dff;
        }
        QSlider::handle:horizontal:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ffffff, stop:1 #a8d0ff);
        }

        /* Default 3D tool / push buttons */
        QToolButton, QPushButton {
            color: #e6eef8;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a3f66, stop:0.48 #1e2f4e, stop:0.52 #182840, stop:1 #152238);
            border-top: 1px solid #5a7eb8;
            border-left: 1px solid #3f5f96;
            border-right: 1px solid #1a2a48;
            border-bottom: 2px solid #0c1528;
            padding: 8px 16px;
            border-radius: 10px;
            font-weight: 600;
            min-height: 22px;
        }
        QToolButton:hover, QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #355280, stop:0.48 #243a60, stop:0.52 #1e3254, stop:1 #1a2c4a);
            border-top: 1px solid #7aa0d8;
        }
        QToolButton:pressed, QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #152238, stop:1 #1e2f4e);
            border-top: 2px solid #0c1528;
            border-bottom: 1px solid #3f5f96;
            padding-top: 9px;
            padding-bottom: 7px;
        }
        QToolButton:disabled, QPushButton:disabled {
            color: #6e829e;
            background: #152238;
            border: 1px solid #243656;
        }

        /* Combined Run/Stop — dark when idle, green while running */
        QToolButton#runStopButton {
            color: #dce6f5;
            min-width: 108px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3a4558, stop:0.45 #262e3c, stop:0.55 #1c222e, stop:1 #141820);
            border-top: 1px solid #6a7588;
            border-left: 1px solid #4a5568;
            border-right: 1px solid #10141c;
            border-bottom: 3px solid #06080c;
        }
        QToolButton#runStopButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a5568, stop:0.45 #323a48, stop:0.55 #262e3c, stop:1 #1c222e);
        }
        QToolButton#runStopButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #10141c, stop:1 #262e3c);
            border-top: 3px solid #06080c;
            border-bottom: 1px solid #4a5568;
        }
        QToolButton#runStopButton:checked {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4ae892, stop:0.45 #2bc46a, stop:0.55 #1fa85a, stop:1 #178a48);
            border-top: 1px solid #9dffc8;
            border-left: 1px solid #5ef0a0;
            border-right: 1px solid #128040;
            border-bottom: 3px solid #0b5a30;
        }
        QToolButton#runStopButton:checked:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #62f0a4, stop:0.45 #34d878, stop:0.55 #26b864, stop:1 #1c9a52);
        }
        QToolButton#runStopButton:checked:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #157a40, stop:1 #22a85a);
            border-top: 3px solid #0b5a30;
            border-bottom: 1px solid #5ef0a0;
        }

        /* Check for Errors — wide glass button */
        QToolButton#checkButton {
            color: #e6eef8;
            min-width: 132px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2f4a78, stop:0.45 #22385e, stop:0.55 #1a2e50, stop:1 #142640);
            border-top: 1px solid #6a98d8;
            border-left: 1px solid #4a78b8;
            border-right: 1px solid #142640;
            border-bottom: 3px solid #0a1428;
        }
        QToolButton#checkButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3a5a90, stop:0.45 #2c446e, stop:0.55 #22385e, stop:1 #1a2e50);
        }
        QToolButton#checkButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #142640, stop:1 #22385e);
            border-top: 3px solid #0a1428;
            border-bottom: 1px solid #4a78b8;
        }

        QStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #101b30, stop:1 #0c1526);
            color: #9eb4d4;
            border-top: 1px solid #2a4068;
            min-height: 26px;
            padding: 2px 8px;
        }
        QStatusBar QLabel {
            color: #9eb4d4;
            padding-left: 4px;
        }
        #statusDot {
            color: #3dd68c;
            font-size: 14px;
            font-weight: 700;
            padding: 0 2px 0 0;
        }

        QTabWidget::pane {
            border: 1px solid #2a3f66;
            border-radius: 0 10px 10px 10px;
            background: rgba(10, 16, 28, 200);
            top: -1px;
        }
        QTabBar::tab {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1a2a48, stop:1 #121e36);
            color: #8aa0c0;
            padding: 9px 16px;
            margin-right: 3px;
            border: 1px solid #2a3f66;
            border-bottom: none;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            min-width: 80px;
        }
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #243a60, stop:1 #0e1830);
            color: #e6eef8;
            border-top: 2px solid #4a8dff;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background: #1e3254;
            color: #c8d7eb;
        }

        QSplitter::handle {
            background: #1a2a48;
        }
        QSplitter::handle:horizontal {
            width: 3px;
        }
        QSplitter::handle:vertical {
            height: 3px;
        }

        QScrollBar:vertical {
            background: #0c1526;
            width: 12px;
            margin: 0;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background: #2a4068;
            min-height: 28px;
            border-radius: 6px;
            border: 1px solid #3a5f9a;
        }
        QScrollBar::handle:vertical:hover {
            background: #3a5f9a;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #0c1526;
            height: 12px;
            margin: 0;
            border-radius: 6px;
        }
        QScrollBar::handle:horizontal {
            background: #2a4068;
            min-width: 28px;
            border-radius: 6px;
            border: 1px solid #3a5f9a;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        QMessageBox {
            background: #121e36;
        }
        QMessageBox QLabel {
            color: #dce6f5;
        }
    )"));
}
