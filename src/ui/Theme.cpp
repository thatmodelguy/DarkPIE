#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace {

DarkPieThemeId g_currentTheme = DarkPieThemeId::NeonRed;

struct ThemePalette {
    QString win0;
    QString win1;
    QString win2;
    QString base;
    QString panel0;
    QString panel1;
    QString text;
    QString muted;
    QString soft;
    QString disabled;
    QString accent;
    QString accentHot;
    QString accentDim;
    QString border;
    QString borderSoft;
    QString selectBg;
    QString tipBg;
    QString menu0;
    QString menu1;
    QString btn0;
    QString btn1;
    QString btn2;
    QString btn3;
    QString btnTop;
    QString btnSide;
    QString btnDeep;
    QString btnHover0;
    QString btnHover1;
    QString runIdle0;
    QString runIdle1;
    QString runIdle2;
    QString runIdle3;
    QString runIdleTop;
    QString runIdleSide;
    QString runActive0;
    QString runActive1;
    QString runActive2;
    QString runActive3;
    QString runActiveTop;
    QString runActiveSide;
    QString runActiveDeep;
    QString check0;
    QString check1;
    QString check2;
    QString check3;
    QString sliderFill0;
    QString sliderFill1;
    QString handle0;
    QString handle1;
    QString debugText;
    QString scroll;
    DarkPieEditorColors editor;
};

ThemePalette paletteFor(DarkPieThemeId id) {
    ThemePalette p;
    switch (id) {
    case DarkPieThemeId::NeonGreen:
        p.win0 = QStringLiteral("#000000");
        p.win1 = QStringLiteral("#041208");
        p.win2 = QStringLiteral("#061a0c");
        p.base = QStringLiteral("#020804");
        p.panel0 = QStringLiteral("#0c1e12");
        p.panel1 = QStringLiteral("#06140a");
        p.text = QStringLiteral("#e4f5e8");
        p.muted = QStringLiteral("#90c8a0");
        p.soft = QStringLiteral("#c8e8d0");
        p.disabled = QStringLiteral("#5a8068");
        p.accent = QStringLiteral("#18e060");
        p.accentHot = QStringLiteral("#40ff80");
        p.accentDim = QStringLiteral("#0e8a3a");
        p.border = QStringLiteral("#1a8a40");
        p.borderSoft = QStringLiteral("#145828");
        p.selectBg = QStringLiteral("#147038");
        p.tipBg = QStringLiteral("#061a0c");
        p.menu0 = QStringLiteral("#0a1a10");
        p.menu1 = QStringLiteral("#040e08");
        p.btn0 = QStringLiteral("#1e4a2c");
        p.btn1 = QStringLiteral("#12321c");
        p.btn2 = QStringLiteral("#0c2414");
        p.btn3 = QStringLiteral("#08180c");
        p.btnTop = QStringLiteral("#30c060");
        p.btnSide = QStringLiteral("#1e8040");
        p.btnDeep = QStringLiteral("#020804");
        p.btnHover0 = QStringLiteral("#2a6a3c");
        p.btnHover1 = QStringLiteral("#184828");
        p.runIdle0 = QStringLiteral("#1a3020");
        p.runIdle1 = QStringLiteral("#102018");
        p.runIdle2 = QStringLiteral("#0a140e");
        p.runIdle3 = QStringLiteral("#060c08");
        p.runIdleTop = QStringLiteral("#3a7050");
        p.runIdleSide = QStringLiteral("#284838");
        p.runActive0 = QStringLiteral("#50ff90");
        p.runActive1 = QStringLiteral("#20e060");
        p.runActive2 = QStringLiteral("#14c050");
        p.runActive3 = QStringLiteral("#0e9038");
        p.runActiveTop = QStringLiteral("#a0ffc8");
        p.runActiveSide = QStringLiteral("#60ff98");
        p.runActiveDeep = QStringLiteral("#085828");
        p.check0 = QStringLiteral("#246838");
        p.check1 = QStringLiteral("#164828");
        p.check2 = QStringLiteral("#103820");
        p.check3 = QStringLiteral("#0a2414");
        p.sliderFill0 = QStringLiteral("#0e6838");
        p.sliderFill1 = QStringLiteral("#28f070");
        p.handle0 = QStringLiteral("#c0ffd8");
        p.handle1 = QStringLiteral("#28e060");
        p.debugText = QStringLiteral("#b0e0b8");
        p.scroll = QStringLiteral("#1a4a2c");
        p.editor = {
            QColor(70, 230, 130),
            QColor(140, 240, 180),
            QColor(190, 230, 120),
            QColor(90, 120, 100),
            QColor(255, 190, 100),
            QColor(8, 20, 12),
            QColor(90, 130, 105),
            QColor(24, 120, 48),
            QColor(210, 255, 220),
        };
        break;
    case DarkPieThemeId::ChromeBlue:
        p.win0 = QStringLiteral("#0a1020");
        p.win1 = QStringLiteral("#0c1528");
        p.win2 = QStringLiteral("#0e1a32");
        p.base = QStringLiteral("#0a101c");
        p.panel0 = QStringLiteral("#15233d");
        p.panel1 = QStringLiteral("#0f1a30");
        p.text = QStringLiteral("#dce6f5");
        p.muted = QStringLiteral("#9eb4d4");
        p.soft = QStringLiteral("#c8d7eb");
        p.disabled = QStringLiteral("#6e829e");
        p.accent = QStringLiteral("#409cff");
        p.accentHot = QStringLiteral("#7cbcff");
        p.accentDim = QStringLiteral("#2a6fd0");
        p.border = QStringLiteral("#2a3f66");
        p.borderSoft = QStringLiteral("#243656");
        p.selectBg = QStringLiteral("#2f5f9e");
        p.tipBg = QStringLiteral("#142038");
        p.menu0 = QStringLiteral("#121e36");
        p.menu1 = QStringLiteral("#0c1526");
        p.btn0 = QStringLiteral("#2a3f66");
        p.btn1 = QStringLiteral("#1e2f4e");
        p.btn2 = QStringLiteral("#182840");
        p.btn3 = QStringLiteral("#152238");
        p.btnTop = QStringLiteral("#5a7eb8");
        p.btnSide = QStringLiteral("#3f5f96");
        p.btnDeep = QStringLiteral("#0c1528");
        p.btnHover0 = QStringLiteral("#355280");
        p.btnHover1 = QStringLiteral("#243a60");
        p.runIdle0 = QStringLiteral("#3a4558");
        p.runIdle1 = QStringLiteral("#262e3c");
        p.runIdle2 = QStringLiteral("#1c222e");
        p.runIdle3 = QStringLiteral("#141820");
        p.runIdleTop = QStringLiteral("#6a7588");
        p.runIdleSide = QStringLiteral("#4a5568");
        p.runActive0 = QStringLiteral("#5ab0ff");
        p.runActive1 = QStringLiteral("#2a88f0");
        p.runActive2 = QStringLiteral("#1a6fd0");
        p.runActive3 = QStringLiteral("#1258a8");
        p.runActiveTop = QStringLiteral("#a8d4ff");
        p.runActiveSide = QStringLiteral("#70b8ff");
        p.runActiveDeep = QStringLiteral("#0a3a70");
        p.check0 = QStringLiteral("#2f4a78");
        p.check1 = QStringLiteral("#22385e");
        p.check2 = QStringLiteral("#1a2e50");
        p.check3 = QStringLiteral("#142640");
        p.sliderFill0 = QStringLiteral("#2a7dff");
        p.sliderFill1 = QStringLiteral("#4ab0ff");
        p.handle0 = QStringLiteral("#e8f2ff");
        p.handle1 = QStringLiteral("#8ec0ff");
        p.debugText = QStringLiteral("#b8d0a8");
        p.scroll = QStringLiteral("#2a4068");
        p.editor = {
            QColor(100, 190, 255),
            QColor(168, 200, 255),
            QColor(110, 220, 150),
            QColor(100, 118, 145),
            QColor(255, 180, 120),
            QColor(14, 24, 42),
            QColor(110, 128, 155),
            QColor(40, 70, 140),
            QColor(210, 230, 255),
        };
        break;
    case DarkPieThemeId::Dark:
        p.win0 = QStringLiteral("#121418");
        p.win1 = QStringLiteral("#16181c");
        p.win2 = QStringLiteral("#1a1d22");
        p.base = QStringLiteral("#0e1014");
        p.panel0 = QStringLiteral("#1e2127");
        p.panel1 = QStringLiteral("#181b20");
        p.text = QStringLiteral("#dce0e6");
        p.muted = QStringLiteral("#9aa3ad");
        p.soft = QStringLiteral("#c4c9d0");
        p.disabled = QStringLiteral("#787c84");
        p.accent = QStringLiteral("#58a6ff");
        p.accentHot = QStringLiteral("#79b8ff");
        p.accentDim = QStringLiteral("#3a5a86");
        p.border = QStringLiteral("#2b3038");
        p.borderSoft = QStringLiteral("#3a414c");
        p.selectBg = QStringLiteral("#3a5a86");
        p.tipBg = QStringLiteral("#1e2127");
        p.menu0 = QStringLiteral("#1e2127");
        p.menu1 = QStringLiteral("#16181c");
        p.btn0 = QStringLiteral("#353b45");
        p.btn1 = QStringLiteral("#2b3038");
        p.btn2 = QStringLiteral("#242830");
        p.btn3 = QStringLiteral("#1e2127");
        p.btnTop = QStringLiteral("#5a6270");
        p.btnSide = QStringLiteral("#3a414c");
        p.btnDeep = QStringLiteral("#101214");
        p.btnHover0 = QStringLiteral("#404650");
        p.btnHover1 = QStringLiteral("#323840");
        p.runIdle0 = QStringLiteral("#3a3f48");
        p.runIdle1 = QStringLiteral("#2a2e36");
        p.runIdle2 = QStringLiteral("#22262c");
        p.runIdle3 = QStringLiteral("#181b20");
        p.runIdleTop = QStringLiteral("#6a7078");
        p.runIdleSide = QStringLiteral("#4a5058");
        p.runActive0 = QStringLiteral("#4a90d0");
        p.runActive1 = QStringLiteral("#2f6fad");
        p.runActive2 = QStringLiteral("#255a90");
        p.runActive3 = QStringLiteral("#1c4874");
        p.runActiveTop = QStringLiteral("#90c0f0");
        p.runActiveSide = QStringLiteral("#68a4e0");
        p.runActiveDeep = QStringLiteral("#123050");
        p.check0 = QStringLiteral("#3a414c");
        p.check1 = QStringLiteral("#2b3038");
        p.check2 = QStringLiteral("#242830");
        p.check3 = QStringLiteral("#1e2127");
        p.sliderFill0 = QStringLiteral("#3a5a86");
        p.sliderFill1 = QStringLiteral("#58a6ff");
        p.handle0 = QStringLiteral("#e8eef5");
        p.handle1 = QStringLiteral("#9ab4d0");
        p.debugText = QStringLiteral("#b8c0a8");
        p.scroll = QStringLiteral("#3a414c");
        p.editor = {
            QColor(108, 182, 255),
            QColor(210, 168, 255),
            QColor(127, 212, 145),
            QColor(110, 118, 129),
            QColor(255, 171, 112),
            QColor(30, 33, 39),
            QColor(120, 124, 132),
            QColor(160, 40, 40),
            QColor(255, 210, 210),
        };
        break;
    case DarkPieThemeId::NeonRed:
    default:
        p.win0 = QStringLiteral("#000000");
        p.win1 = QStringLiteral("#120204");
        p.win2 = QStringLiteral("#1a0508");
        p.base = QStringLiteral("#060000");
        p.panel0 = QStringLiteral("#22060a");
        p.panel1 = QStringLiteral("#100204");
        p.text = QStringLiteral("#f0e4e4");
        p.muted = QStringLiteral("#c8a0a4");
        p.soft = QStringLiteral("#e8d0d0");
        p.disabled = QStringLiteral("#785055");
        p.accent = QStringLiteral("#e6141e");
        p.accentHot = QStringLiteral("#ff3038");
        p.accentDim = QStringLiteral("#8a0c12");
        p.border = QStringLiteral("#7a1018");
        p.borderSoft = QStringLiteral("#6a1018");
        p.selectBg = QStringLiteral("#8a1218");
        p.tipBg = QStringLiteral("#1a0508");
        p.menu0 = QStringLiteral("#1a0508");
        p.menu1 = QStringLiteral("#0c0204");
        p.btn0 = QStringLiteral("#4a1418");
        p.btn1 = QStringLiteral("#2e0a0e");
        p.btn2 = QStringLiteral("#220608");
        p.btn3 = QStringLiteral("#160406");
        p.btnTop = QStringLiteral("#c02830");
        p.btnSide = QStringLiteral("#8a181e");
        p.btnDeep = QStringLiteral("#050000");
        p.btnHover0 = QStringLiteral("#6a1c22");
        p.btnHover1 = QStringLiteral("#3e1014");
        p.runIdle0 = QStringLiteral("#3a1418");
        p.runIdle1 = QStringLiteral("#22080c");
        p.runIdle2 = QStringLiteral("#160406");
        p.runIdle3 = QStringLiteral("#0c0204");
        p.runIdleTop = QStringLiteral("#8a3038");
        p.runIdleSide = QStringLiteral("#5a181e");
        p.runActive0 = QStringLiteral("#ff4048");
        p.runActive1 = QStringLiteral("#e01018");
        p.runActive2 = QStringLiteral("#c00c14");
        p.runActive3 = QStringLiteral("#8a080e");
        p.runActiveTop = QStringLiteral("#ff9098");
        p.runActiveSide = QStringLiteral("#ff5058");
        p.runActiveDeep = QStringLiteral("#3a0406");
        p.check0 = QStringLiteral("#5a181e");
        p.check1 = QStringLiteral("#3a0e12");
        p.check2 = QStringLiteral("#2a0a0e");
        p.check3 = QStringLiteral("#1a0608");
        p.sliderFill0 = QStringLiteral("#8a0c12");
        p.sliderFill1 = QStringLiteral("#ff2028");
        p.handle0 = QStringLiteral("#ff9098");
        p.handle1 = QStringLiteral("#e01018");
        p.debugText = QStringLiteral("#e0b0b0");
        p.scroll = QStringLiteral("#4a1418");
        p.editor = {
            QColor(255, 90, 96),
            QColor(255, 160, 140),
            QColor(255, 190, 120),
            QColor(120, 90, 95),
            QColor(255, 140, 100),
            QColor(16, 4, 6),
            QColor(120, 80, 85),
            QColor(150, 24, 32),
            QColor(255, 210, 210),
        };
        break;
    }
    return p;
}

QString buildStyleSheet(const ThemePalette& t) {
    QString css = QStringLiteral(R"(
        QMainWindow, QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 @win0@, stop:0.45 @win1@, stop:1 @win2@);
            color: @text@;
        }
        QToolTip {
            background: @tipBg@;
            color: @text@;
            border: 1px solid @accentHot@;
            padding: 6px 10px;
            border-radius: 4px;
        }

        QPlainTextEdit, QTextEdit {
            background: @base@;
            color: @text@;
            border: 1px solid @border@;
            border-radius: 4px;
            selection-background-color: @selectBg@;
            selection-color: #ffffff;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 13px;
            padding: 4px;
        }
        QPlainTextEdit:focus, QTextEdit:focus {
            border: 1px solid @accentHot@;
        }

        QMenuBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @panel0@, stop:1 @panel1@);
            color: @text@;
            border-bottom: 1px solid @border@;
            padding: 2px 6px;
            spacing: 2px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 12px;
            border-radius: 3px;
            margin: 2px 1px;
        }
        QMenuBar::item:selected {
            background: rgba(255,255,255,40);
            color: #ffffff;
        }
        QMenuBar::item:pressed {
            background: rgba(255,255,255,70);
        }
        QMenu {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @menu0@, stop:1 @menu1@);
            color: @text@;
            border: 1px solid @accentHot@;
            border-radius: 4px;
            padding: 6px;
        }
        QMenu::item {
            padding: 7px 28px 7px 16px;
            border-radius: 3px;
            margin: 1px 2px;
        }
        QMenu::item:selected {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 @accent@, stop:1 @accentDim@);
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: @borderSoft@;
            margin: 5px 10px;
        }
        QMenu::indicator {
            width: 14px;
            height: 14px;
            margin-left: 6px;
        }

        #mainButtonBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @panel0@, stop:1 @panel1@);
            border: none;
            border-bottom: 1px solid @border@;
        }
        #mainButtonBar QLabel {
            color: @muted@;
            font-weight: 600;
            font-size: 12px;
        }
        #mainButtonBar #micIcon {
            color: @accentHot@;
            font-weight: 400;
            font-size: 14px;
            padding: 0 2px 0 0;
            min-width: 18px;
        }
        #mainButtonBar #micPercent {
            color: @soft@;
            min-width: 34px;
        }

        #findBar {
            background: @panel1@;
            border-top: 1px solid @borderSoft@;
            border-bottom: 1px solid @borderSoft@;
        }
        #findBar QLabel {
            color: @muted@;
            font-weight: 600;
        }

        #debugPanel { background: transparent; }
        #debugHeader {
            color: @muted@;
            font-weight: 700;
            font-size: 12px;
            letter-spacing: 0.4px;
            padding: 2px 2px 0 4px;
        }
        #debugOutput {
            background: @base@;
            border: 1px solid @border@;
            border-radius: 4px;
            color: @debugText@;
        }

        QLineEdit {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @base@, stop:1 @panel1@);
            color: @text@;
            border: 1px solid @border@;
            padding: 6px 10px;
            border-radius: 4px;
            selection-background-color: @selectBg@;
        }
        QLineEdit:focus {
            border: 1px solid @accentHot@;
            background: @panel1@;
        }

        QCheckBox {
            color: @soft@;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid @border@;
            background: @panel1@;
        }
        QCheckBox::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @accentHot@, stop:1 @accentDim@);
            border: 1px solid @accentHot@;
        }

        QSlider::groove:horizontal {
            height: 8px;
            border-radius: 2px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 @panel1@, stop:1 @menu0@);
            border: 1px solid @borderSoft@;
        }
        QSlider::sub-page:horizontal {
            border-radius: 2px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 @sliderFill0@, stop:1 @sliderFill1@);
        }
        QSlider::add-page:horizontal {
            border-radius: 2px;
            background: transparent;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @handle0@, stop:1 @handle1@);
            border: 1px solid @accentHot@;
        }
        QSlider::handle:horizontal:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ffffff, stop:1 @handle0@);
        }

        QToolButton, QPushButton {
            color: @text@;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @btn0@, stop:0.48 @btn1@, stop:0.52 @btn2@, stop:1 @btn3@);
            border-top: 1px solid @btnTop@;
            border-left: 1px solid @btnSide@;
            border-right: 1px solid @btnDeep@;
            border-bottom: 2px solid @btnDeep@;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: 600;
            min-height: 22px;
        }
        QToolButton:hover, QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @btnHover0@, stop:0.48 @btnHover1@, stop:0.52 @btn1@, stop:1 @btn2@);
            border-top: 1px solid @accentHot@;
        }
        QToolButton:pressed, QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @btn3@, stop:1 @btn1@);
            border-top: 2px solid @btnDeep@;
            border-bottom: 1px solid @btnSide@;
            padding-top: 9px;
            padding-bottom: 7px;
        }
        QToolButton:disabled, QPushButton:disabled {
            color: @disabled@;
            background: @btn3@;
            border: 1px solid @borderSoft@;
        }

        QToolButton#runStopButton {
            color: @soft@;
            min-width: 108px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @runIdle0@, stop:0.45 @runIdle1@, stop:0.55 @runIdle2@, stop:1 @runIdle3@);
            border-top: 1px solid @runIdleTop@;
            border-left: 1px solid @runIdleSide@;
            border-right: 1px solid @runIdle3@;
            border-bottom: 3px solid #000000;
        }
        QToolButton#runStopButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @btnHover0@, stop:0.45 @btnHover1@, stop:0.55 @runIdle1@, stop:1 @runIdle2@);
            border-top: 1px solid @accentHot@;
        }
        QToolButton#runStopButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @runIdle3@, stop:1 @runIdle1@);
            border-top: 3px solid #000000;
            border-bottom: 1px solid @runIdleSide@;
        }
        QToolButton#runStopButton:checked {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @runActive0@, stop:0.45 @runActive1@, stop:0.55 @runActive2@, stop:1 @runActive3@);
            border-top: 1px solid @runActiveTop@;
            border-left: 1px solid @runActiveSide@;
            border-right: 1px solid @runActiveDeep@;
            border-bottom: 3px solid @runActiveDeep@;
        }
        QToolButton#runStopButton:checked:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @accentHot@, stop:0.45 @runActive0@, stop:0.55 @runActive1@, stop:1 @runActive2@);
        }
        QToolButton#runStopButton:checked:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @runActive3@, stop:1 @runActive1@);
            border-top: 3px solid @runActiveDeep@;
            border-bottom: 1px solid @runActiveSide@;
        }

        QToolButton#checkButton {
            color: @text@;
            min-width: 132px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @check0@, stop:0.45 @check1@, stop:0.55 @check2@, stop:1 @check3@);
            border-top: 1px solid @btnTop@;
            border-left: 1px solid @btnSide@;
            border-right: 1px solid @check3@;
            border-bottom: 3px solid @btnDeep@;
        }
        QToolButton#checkButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @btnHover0@, stop:0.45 @btnHover1@, stop:0.55 @check1@, stop:1 @check2@);
            border-top: 1px solid @accentHot@;
        }
        QToolButton#checkButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @check3@, stop:1 @check1@);
            border-top: 3px solid @btnDeep@;
            border-bottom: 1px solid @btnSide@;
        }

        QStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @panel1@, stop:1 @base@);
            color: @muted@;
            border-top: 1px solid @borderSoft@;
            min-height: 26px;
            padding: 2px 8px;
        }
        QStatusBar QLabel {
            color: @muted@;
            padding-left: 4px;
        }
        #statusDot {
            color: @accentHot@;
            font-size: 14px;
            font-weight: 700;
            padding: 0 2px 0 0;
        }

        QTabWidget::pane {
            border: 1px solid @border@;
            border-radius: 0 4px 4px 4px;
            background: @base@;
            top: -1px;
        }
        QTabBar::tab {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @menu0@, stop:1 @panel1@);
            color: @muted@;
            padding: 9px 16px;
            margin-right: 3px;
            border: 1px solid @borderSoft@;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            min-width: 80px;
        }
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 @panel0@, stop:1 @base@);
            color: @text@;
            border-top: 2px solid @accentHot@;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background: @menu0@;
            color: @soft@;
        }

        QSplitter::handle { background: @borderSoft@; }
        QSplitter::handle:horizontal { width: 3px; }
        QSplitter::handle:vertical { height: 3px; }

        QScrollBar:vertical {
            background: @base@;
            width: 12px;
            margin: 0;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical {
            background: @scroll@;
            min-height: 28px;
            border-radius: 3px;
            border: 1px solid @btnSide@;
        }
        QScrollBar::handle:vertical:hover { background: @btnSide@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: @base@;
            height: 12px;
            margin: 0;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal {
            background: @scroll@;
            min-width: 28px;
            border-radius: 3px;
            border: 1px solid @btnSide@;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        QMessageBox { background: @panel1@; }
        QMessageBox QLabel { color: @text@; }
    )");

    const auto put = [&](const char* key, const QString& value) {
        css.replace(QLatin1String(key), value);
    };
    put("@win0@", t.win0);
    put("@win1@", t.win1);
    put("@win2@", t.win2);
    put("@base@", t.base);
    put("@panel0@", t.panel0);
    put("@panel1@", t.panel1);
    put("@text@", t.text);
    put("@muted@", t.muted);
    put("@soft@", t.soft);
    put("@disabled@", t.disabled);
    put("@accentHot@", t.accentHot);
    put("@accentDim@", t.accentDim);
    put("@accent@", t.accent);
    put("@borderSoft@", t.borderSoft);
    put("@border@", t.border);
    put("@selectBg@", t.selectBg);
    put("@tipBg@", t.tipBg);
    put("@menu0@", t.menu0);
    put("@menu1@", t.menu1);
    put("@btn0@", t.btn0);
    put("@btn1@", t.btn1);
    put("@btn2@", t.btn2);
    put("@btn3@", t.btn3);
    put("@btnTop@", t.btnTop);
    put("@btnSide@", t.btnSide);
    put("@btnDeep@", t.btnDeep);
    put("@btnHover0@", t.btnHover0);
    put("@btnHover1@", t.btnHover1);
    put("@runIdle0@", t.runIdle0);
    put("@runIdle1@", t.runIdle1);
    put("@runIdle2@", t.runIdle2);
    put("@runIdle3@", t.runIdle3);
    put("@runIdleTop@", t.runIdleTop);
    put("@runIdleSide@", t.runIdleSide);
    put("@runActive0@", t.runActive0);
    put("@runActive1@", t.runActive1);
    put("@runActive2@", t.runActive2);
    put("@runActive3@", t.runActive3);
    put("@runActiveTop@", t.runActiveTop);
    put("@runActiveSide@", t.runActiveSide);
    put("@runActiveDeep@", t.runActiveDeep);
    put("@check0@", t.check0);
    put("@check1@", t.check1);
    put("@check2@", t.check2);
    put("@check3@", t.check3);
    put("@sliderFill0@", t.sliderFill0);
    put("@sliderFill1@", t.sliderFill1);
    put("@handle0@", t.handle0);
    put("@handle1@", t.handle1);
    put("@debugText@", t.debugText);
    put("@scroll@", t.scroll);
    return css;
}

} // namespace

QString darkPieThemeName(DarkPieThemeId id) {
    switch (id) {
    case DarkPieThemeId::NeonRed: return QStringLiteral("Neon Red");
    case DarkPieThemeId::NeonGreen: return QStringLiteral("Neon Green");
    case DarkPieThemeId::ChromeBlue: return QStringLiteral("Chrome Blue");
    case DarkPieThemeId::Dark: return QStringLiteral("Dark");
    }
    return QStringLiteral("Neon Red");
}

QString darkPieThemeKey(DarkPieThemeId id) {
    switch (id) {
    case DarkPieThemeId::NeonRed: return QStringLiteral("neonRed");
    case DarkPieThemeId::NeonGreen: return QStringLiteral("neonGreen");
    case DarkPieThemeId::ChromeBlue: return QStringLiteral("chromeBlue");
    case DarkPieThemeId::Dark: return QStringLiteral("dark");
    }
    return QStringLiteral("neonRed");
}

DarkPieThemeId darkPieThemeFromKey(const QString& key) {
    if (key == QStringLiteral("neonGreen")) return DarkPieThemeId::NeonGreen;
    if (key == QStringLiteral("chromeBlue")) return DarkPieThemeId::ChromeBlue;
    if (key == QStringLiteral("dark")) return DarkPieThemeId::Dark;
    return DarkPieThemeId::NeonRed;
}

DarkPieThemeId loadDarkPieThemeId() {
    return darkPieThemeFromKey(QSettings().value(QStringLiteral("uiTheme"), QStringLiteral("neonRed")).toString());
}

void saveDarkPieThemeId(DarkPieThemeId id) {
    QSettings().setValue(QStringLiteral("uiTheme"), darkPieThemeKey(id));
}

DarkPieThemeId currentDarkPieThemeId() { return g_currentTheme; }

DarkPieEditorColors darkPieEditorColors() { return paletteFor(g_currentTheme).editor; }

void applyDarkPieTheme(QApplication& app, DarkPieThemeId id) {
    g_currentTheme = id;
    const ThemePalette t = paletteFor(id);

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    p.setColor(QPalette::Window, QColor(t.win1));
    p.setColor(QPalette::WindowText, QColor(t.text));
    p.setColor(QPalette::Base, QColor(t.base));
    p.setColor(QPalette::AlternateBase, QColor(t.panel0));
    p.setColor(QPalette::Text, QColor(t.text));
    p.setColor(QPalette::Button, QColor(t.panel0));
    p.setColor(QPalette::ButtonText, QColor(t.text));
    p.setColor(QPalette::ToolTipBase, QColor(t.tipBg));
    p.setColor(QPalette::ToolTipText, QColor(t.text));
    p.setColor(QPalette::Highlight, QColor(t.accent));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::Link, QColor(t.accentHot));
    p.setColor(QPalette::PlaceholderText, QColor(t.disabled));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(t.disabled));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(t.disabled));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(t.disabled));
    app.setPalette(p);
    app.setStyleSheet(buildStyleSheet(t));
}
