#pragma once

#include <QColor>
#include <QString>

class QApplication;

enum class DarkPieThemeId {
    NeonRed,
    NeonGreen,
    ChromeBlue,
    Dark,
};

struct DarkPieEditorColors {
    QColor keyword;
    QColor function;
    QColor string;
    QColor comment;
    QColor number;
    QColor gutterBg;
    QColor gutterText;
    QColor gutterErrorBg;
    QColor gutterErrorText;
};

QString darkPieThemeName(DarkPieThemeId id);
QString darkPieThemeKey(DarkPieThemeId id);
DarkPieThemeId darkPieThemeFromKey(const QString& key);
DarkPieThemeId loadDarkPieThemeId();
void saveDarkPieThemeId(DarkPieThemeId id);
DarkPieThemeId currentDarkPieThemeId();
void applyDarkPieTheme(QApplication& app, DarkPieThemeId id);
DarkPieEditorColors darkPieEditorColors();
