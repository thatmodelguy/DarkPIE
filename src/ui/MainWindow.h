#pragma once

#include "runtime/Engine.h"

#include <QMainWindow>
#include <QStringList>
#include <QTabWidget>

class ScriptEditor;
class QPlainTextEdit;
class QLabel;
class QAction;
class QActionGroup;
class QMenu;
class QLineEdit;
class QCheckBox;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool openPath(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void runScript();
    void stopScript();
    void onEngineStatus();
    void openRecent();
    void clearRecent();
    void onMicrophoneChosen(QAction* action);
    void rebuildMicrophoneMenu();
    void checkScript();
    void showFindReplace();
    void findNext();
    void findPrevious();
    void replaceOne();
    void replaceAll();
    void openSpeechTraining();
    void openMicrophoneSetup();
    void openSpeechSettings();
    void toggleSleepWake();

private:
    ScriptEditor* currentEditor() const;
    QString currentPath() const;
    void setCurrentPath(const QString& path);
    bool maybeSave();
    void addTab(const QString& path, const QString& text);
    void updateTitle();
    void updateRunningUi();
    bool findInEditor(bool forward);
    void rememberRecent(const QString& path);
    void rebuildRecentMenu();
    QStringList loadRecent() const;
    void saveRecent(const QStringList& files) const;
    QString scriptDialogDirectory() const;
    void noteScriptDirectory(const QString& filePath);

    QTabWidget* tabs_ = nullptr;
    QPlainTextEdit* debug_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QAction* runAct_ = nullptr;
    QAction* stopAct_ = nullptr;
    QAction* checkAct_ = nullptr;
    QAction* findReplaceAct_ = nullptr;
    QWidget* findBar_ = nullptr;
    QLineEdit* findEdit_ = nullptr;
    QLineEdit* replaceEdit_ = nullptr;
    QCheckBox* matchCaseCheck_ = nullptr;
    QMenu* recentMenu_ = nullptr;
    QMenu* micMenu_ = nullptr;
    QActionGroup* micGroup_ = nullptr;
    QAction* sleepWakeAct_ = nullptr;
    Engine engine_;
};
