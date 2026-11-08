#include "MainWindow.h"
#include "ScriptEditor.h"
#include "Theme.h"

#include "devices/SpeechDevice.h"
#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "lang/ScriptCheck.h"

#include <algorithm>

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString windowsSystem32() {
    return QDir::toNativeSeparators(QString::fromLocal8Bit(qgetenv("WINDIR")) + QStringLiteral("/System32"));
}

bool launchDetached(const QString& program, const QStringList& args = {}) {
    return QProcess::startDetached(program, args);
}

bool launchSpeechWizard(const QString& mode) {
    const QString wiz = windowsSystem32() + QStringLiteral("\\Speech\\SpeechUX\\SpeechUXWiz.exe");
    if (QFileInfo::exists(wiz) && launchDetached(wiz, {mode})) return true;
    const QString dll = windowsSystem32() + QStringLiteral("\\Speech\\SpeechUX\\SpeechUX.dll");
    if (QFileInfo::exists(dll) &&
        launchDetached(QStringLiteral("rundll32.exe"), {dll + QStringLiteral(",RunWizard"), mode}))
        return true;
    return false;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("DarkPIE"));
    resize(1100, 720);

    auto* root = new QWidget(this);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    auto* buttonBar = new QWidget(root);
    buttonBar->setObjectName(QStringLiteral("mainButtonBar"));
    auto* barLay = new QHBoxLayout(buttonBar);
    barLay->setContentsMargins(14, 10, 14, 10);
    barLay->setSpacing(12);

    auto* micVol = new QWidget(buttonBar);
    auto* micLay = new QHBoxLayout(micVol);
    micLay->setContentsMargins(0, 0, 0, 0);
    micLay->setSpacing(6);
    auto* micIcon = new QLabel(micVol);
    micIcon->setObjectName(QStringLiteral("micIcon"));
    micIcon->setAlignment(Qt::AlignCenter);
    micIcon->setToolTip(QStringLiteral("Microphone volume"));
    {
        QFont iconFont(QStringLiteral("Segoe Fluent Icons"));
        if (!QFontInfo(iconFont).exactMatch())
            iconFont = QFont(QStringLiteral("Segoe MDL2 Assets"));
        if (QFontInfo(iconFont).family().contains(QStringLiteral("Segoe"), Qt::CaseInsensitive)) {
            iconFont.setPointSize(13);
            micIcon->setFont(iconFont);
            micIcon->setText(QChar(0xE720)); // Microphone
        } else {
            micIcon->setText(QString::fromUtf8(u8"\U0001F3A4"));
        }
    }
    auto* micLabel = new QLabel(micVol);
    micLabel->setObjectName(QStringLiteral("micPercent"));
    micLabel->setMinimumWidth(micLabel->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
    micLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* micSlider = new QSlider(Qt::Horizontal, micVol);
    micSlider->setRange(0, 100);
    micSlider->setMinimumWidth(72);
    micSlider->setMaximumWidth(160);
    micSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const QString micTip = QStringLiteral(
        "Microphone volume used while a script is running. It is set when you press Run, before the "
        "microphone opens, and the previous volume is put back when the script stops.");
    micSlider->setToolTip(micTip);
    micIcon->setToolTip(micTip);
    micLabel->setToolTip(micTip);
    micLay->addWidget(micIcon);
    micLay->addWidget(micLabel);
    micLay->addWidget(micSlider, 1);
    micVol->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    micVol->setMaximumWidth(240);

    runStopBtn_ = new QToolButton(buttonBar);
    auto* checkBtn = new QToolButton(buttonBar);
    runStopBtn_->setObjectName(QStringLiteral("runStopButton"));
    checkBtn->setObjectName(QStringLiteral("checkButton"));
    runStopBtn_->setCursor(Qt::PointingHandCursor);
    checkBtn->setCursor(Qt::PointingHandCursor);
    runStopBtn_->setCheckable(true);
    runStopBtn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    runStopBtn_->setText(QStringLiteral("Run"));
    runStopBtn_->setToolTip(QStringLiteral("Run the script"));
    runStopBtn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    checkBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(runStopBtn_, &QToolButton::clicked, this, &MainWindow::toggleRunStop);

    // Mic left, Run/Stop centered, Check right — real columns so they never overlap.
    barLay->addWidget(micVol, 0, Qt::AlignVCenter | Qt::AlignLeft);
    barLay->addStretch(1);
    barLay->addWidget(runStopBtn_, 0, Qt::AlignVCenter);
    barLay->addStretch(1);
    barLay->addWidget(checkBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

    auto* content = new QWidget(root);
    auto* contentLay = new QVBoxLayout(content);
    contentLay->setContentsMargins(10, 8, 10, 8);
    contentLay->setSpacing(8);

    auto* splitter = new QSplitter(Qt::Vertical, content);
    auto* editorPane = new QWidget;
    editorPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* editorLay = new QVBoxLayout(editorPane);
    editorLay->setContentsMargins(0, 0, 0, 0);
    editorLay->setSpacing(0);

    tabs_ = new QTabWidget(editorPane);
    tabs_->setDocumentMode(true);
    tabs_->setTabsClosable(true);
    tabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    findBar_ = new QWidget(editorPane);
    findBar_->setObjectName(QStringLiteral("findBar"));
    findBar_->setVisible(false);
    auto* findLay = new QHBoxLayout(findBar_);
    findLay->setContentsMargins(10, 8, 10, 8);
    findLay->setSpacing(8);
    findLay->addWidget(new QLabel(QStringLiteral("Find"), findBar_));
    findEdit_ = new QLineEdit(findBar_);
    findEdit_->setPlaceholderText(QStringLiteral("Search in script"));
    findEdit_->setClearButtonEnabled(true);
    findLay->addWidget(findEdit_, 1);
    findLay->addWidget(new QLabel(QStringLiteral("Replace"), findBar_));
    replaceEdit_ = new QLineEdit(findBar_);
    replaceEdit_->setPlaceholderText(QStringLiteral("Replace with"));
    replaceEdit_->setClearButtonEnabled(true);
    findLay->addWidget(replaceEdit_, 1);
    matchCaseCheck_ = new QCheckBox(QStringLiteral("Match case"), findBar_);
    findLay->addWidget(matchCaseCheck_);
    auto* findNextBtn = new QPushButton(QStringLiteral("Find Next"), findBar_);
    auto* replaceBtn = new QPushButton(QStringLiteral("Replace"), findBar_);
    auto* replaceAllBtn = new QPushButton(QStringLiteral("Replace All"), findBar_);
    findLay->addWidget(findNextBtn);
    findLay->addWidget(replaceBtn);
    findLay->addWidget(replaceAllBtn);
    auto* closeFindBtn = new QPushButton(QStringLiteral("Close"), findBar_);
    findLay->addWidget(closeFindBtn);
    connect(closeFindBtn, &QPushButton::clicked, this, [this] {
        if (findBar_) findBar_->setVisible(false);
        if (auto* ed = currentEditor()) ed->setFocus();
    });
    connect(findNextBtn, &QPushButton::clicked, this, &MainWindow::findNext);
    connect(replaceBtn, &QPushButton::clicked, this, &MainWindow::replaceOne);
    connect(replaceAllBtn, &QPushButton::clicked, this, &MainWindow::replaceAll);
    connect(findEdit_, &QLineEdit::returnPressed, this, &MainWindow::findNext);
    connect(replaceEdit_, &QLineEdit::returnPressed, this, &MainWindow::replaceOne);

    editorLay->addWidget(tabs_, 1);
    editorLay->addWidget(findBar_); // sits under the Scripts tabs, above debug output

    auto* debugPanel = new QWidget;
    debugPanel->setObjectName(QStringLiteral("debugPanel"));
    auto* debugLay = new QVBoxLayout(debugPanel);
    debugLay->setContentsMargins(0, 0, 0, 0);
    debugLay->setSpacing(4);
    auto* debugHeader = new QLabel(QStringLiteral("Debug Output"), debugPanel);
    debugHeader->setObjectName(QStringLiteral("debugHeader"));
    debug_ = new QPlainTextEdit(debugPanel);
    debug_->setObjectName(QStringLiteral("debugOutput"));
    debug_->setReadOnly(true);
    debug_->setPlaceholderText(QStringLiteral("Debug output"));
    debug_->setMaximumBlockCount(2000);
    debug_->setMinimumHeight(72);
    debug_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    debugLay->addWidget(debugHeader);
    debugLay->addWidget(debug_, 1);
    splitter->addWidget(editorPane);
    splitter->addWidget(debugPanel);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({560, 100});

    contentLay->addWidget(splitter, 1);
    rootLay->addWidget(buttonBar);
    rootLay->addWidget(content, 1);
    setCentralWidget(root);

    connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int i) {
        tabs_->setCurrentIndex(i);
        if (maybeSave()) {
            auto* w = tabs_->widget(i);
            tabs_->removeTab(i);
            delete w;
            if (tabs_->count() == 0) newFile();
        }
    });

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&New"), QKeySequence::New, this, &MainWindow::newFile);
    fileMenu->addAction(QStringLiteral("&Open..."), QKeySequence::Open, this, &MainWindow::openFile);
    fileMenu->addAction(QStringLiteral("&Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(QStringLiteral("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    recentMenu_ = fileMenu->addMenu(QStringLiteral("Recent &Scripts"));
    rebuildRecentMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* runMenu = menuBar()->addMenu(QStringLiteral("&Script"));
    runAct_ = runMenu->addAction(QStringLiteral("&Run"), this, &MainWindow::runScript);
    runAct_->setToolTip(QStringLiteral("Run the script"));
    stopAct_ = runMenu->addAction(QStringLiteral("&Stop"), this, &MainWindow::stopScript);
    stopAct_->setToolTip(QStringLiteral("Stop the script"));
    stopAct_->setEnabled(false);
    runMenu->addSeparator();
    checkAct_ = runMenu->addAction(QStringLiteral("&Check for Errors"), this, &MainWindow::checkScript);
    checkAct_->setToolTip(QStringLiteral("Scan the script for errors"));
    runMenu->addSeparator();
    findReplaceAct_ = runMenu->addAction(QStringLiteral("Search and &Replace"), this, &MainWindow::showFindReplace);
    findReplaceAct_->setShortcut(QKeySequence::Find);
    findReplaceAct_->setToolTip(QStringLiteral("Find and replace text in the current script (Ctrl+F)"));
    auto* findNextAct = runMenu->addAction(QStringLiteral("Find &Next"), this, &MainWindow::findNext);
    findNextAct->setShortcut(QKeySequence::FindNext);
    auto* findPrevAct = runMenu->addAction(QStringLiteral("Find &Previous"), this, &MainWindow::findPrevious);
    findPrevAct->setShortcut(QKeySequence::FindPrevious);
    auto* replaceMenuAct = runMenu->addAction(QStringLiteral("R&eplace..."), this, [this] {
        showFindReplace();
        if (replaceEdit_) replaceEdit_->setFocus();
        if (replaceEdit_) replaceEdit_->selectAll();
    });
    replaceMenuAct->setShortcut(QKeySequence::Replace);

    auto* speechMenu = menuBar()->addMenu(QStringLiteral("S&peech"));
    sleepWakeAct_ = speechMenu->addAction(QStringLiteral("&Sleep / Wake Commands"));
    sleepWakeAct_->setCheckable(true);
    sleepWakeAct_->setToolTip(QStringLiteral("Say \"go to sleep\" to pause script voice commands. Say \"wake up\" to resume. The microphone stays listening for wake up."));
    connect(sleepWakeAct_, &QAction::toggled, this, &MainWindow::toggleSleepWake);
    speechMenu->addSeparator();
    speechMenu->addAction(QStringLiteral("&Train Speech Recognition..."), this, &MainWindow::openSpeechTraining);
    speechMenu->addAction(QStringLiteral("Set Up &Microphone..."), this, &MainWindow::openMicrophoneSetup);
    speechMenu->addSeparator();
    speechMenu->addAction(QStringLiteral("Windows Speech &Settings..."), this, &MainWindow::openSpeechSettings);

    micMenu_ = menuBar()->addMenu(QStringLiteral("&Microphone"));
    micGroup_ = new QActionGroup(this);
    micGroup_->setExclusive(true);
    connect(micGroup_, &QActionGroup::triggered, this, &MainWindow::onMicrophoneChosen);
    connect(micMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildMicrophoneMenu);

    auto* themeMenu = menuBar()->addMenu(QStringLiteral("&Theme"));
    themeGroup_ = new QActionGroup(this);
    themeGroup_->setExclusive(true);
    const DarkPieThemeId activeTheme = currentDarkPieThemeId();
    const DarkPieThemeId themeIds[] = {
        DarkPieThemeId::NeonRed,
        DarkPieThemeId::NeonGreen,
        DarkPieThemeId::ChromeBlue,
        DarkPieThemeId::Dark,
    };
    for (DarkPieThemeId id : themeIds) {
        auto* act = themeMenu->addAction(darkPieThemeName(id));
        act->setCheckable(true);
        act->setData(static_cast<int>(id));
        act->setChecked(id == activeTheme);
        themeGroup_->addAction(act);
    }
    connect(themeGroup_, &QActionGroup::triggered, this, &MainWindow::onThemeChosen);
    {
        QSettings settings;
        if (settings.value(QStringLiteral("micVolumeHold")).toBool()) {
            const QString endpoint = settings.value(QStringLiteral("micVolumeHoldEndpoint")).toString();
            const float level = settings.value(QStringLiteral("micVolumeHoldLevel"), 1.0).toFloat();
            if (endpoint.isEmpty() || SpeechDevice::forceEndpointVolume(endpoint.toStdString(), level)) {
                settings.remove(QStringLiteral("micVolumeHold"));
                settings.remove(QStringLiteral("micVolumeHoldEndpoint"));
                settings.remove(QStringLiteral("micVolumeHoldLevel"));
            }
        }
        const int micPct = std::clamp(settings.value(QStringLiteral("micVolumePercent"), 100).toInt(), 0, 100);
        engine_.bus().speech().setCaptureVolumePercent(micPct);
        micLabel->setText(QStringLiteral("%1%").arg(micPct));
        {
            QSignalBlocker block(micSlider);
            micSlider->setValue(micPct);
        }
        connect(micSlider, &QSlider::valueChanged, this, [this, micLabel](int value) {
            micLabel->setText(QStringLiteral("%1%").arg(value));
            engine_.bus().speech().setCaptureVolumePercent(value);
            QSettings().setValue(QStringLiteral("micVolumePercent"), value);
        });
        engine_.bus().speech().setMicrophoneId(settings.value(QStringLiteral("microphoneId")).toString().toStdString());
        const bool sleepWake = settings.value(QStringLiteral("sleepWakeEnabled"), true).toBool();
        engine_.bus().speech().setSleepWakeEnabled(sleepWake);
        if (sleepWakeAct_) {
            QSignalBlocker block(sleepWakeAct_);
            sleepWakeAct_->setChecked(sleepWake);
        }
    }
    rebuildMicrophoneMenu();

    checkBtn->setDefaultAction(checkAct_);
    checkBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* statusDot = new QLabel(QStringLiteral("●"));
    statusDot->setObjectName(QStringLiteral("statusDot"));
    statusLabel_ = new QLabel(QStringLiteral("Stopped"));
    statusBar()->addWidget(statusDot);
    statusBar()->addWidget(statusLabel_, 1);

    connect(&engine_, &Engine::debugMessage, this, [this](const QString& t) {
        debug_->appendPlainText(t);
    });
    connect(&engine_, &Engine::errorMessage, this, [this](const QString& t) {
        debug_->appendPlainText(QStringLiteral("[!] ") + t);
    });
    connect(&engine_, &Engine::statusChanged, this, &MainWindow::onEngineStatus);
    connect(&engine_, &Engine::hidePieRequested, this, [this] { hide(); });
    connect(&engine_, &Engine::showPieRequested, this, [this] { show(); raise(); activateWindow(); });
    connect(&engine_, &Engine::exitPieRequested, this, [] { qApp->quit(); });
    connect(&engine_, &Engine::minimizePieRequested, this, [this] { showMinimized(); });
    connect(&engine_, &Engine::maximizePieRequested, this, [this] { showMaximized(); });
    connect(&engine_, &Engine::restorePieRequested, this, [this] { showNormal(); raise(); activateWindow(); });
    connect(&engine_, &Engine::flashPieRequested, this, [this] { QApplication::alert(this); });

    newFile();
    const auto applySplit = [splitter] {
        splitter->setSizes({560, 100});
    };
    applySplit();
    QTimer::singleShot(0, this, applySplit);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        engine_.stop();
        event->accept();
    } else {
        event->ignore();
    }
}

ScriptEditor* MainWindow::currentEditor() const {
    return qobject_cast<ScriptEditor*>(tabs_->currentWidget());
}

QString MainWindow::currentPath() const {
    return tabs_->tabBar()->tabData(tabs_->currentIndex()).toString();
}

void MainWindow::setCurrentPath(const QString& path) {
    int i = tabs_->currentIndex();
    tabs_->tabBar()->setTabData(i, path);
    tabs_->setTabText(i, path.isEmpty() ? QStringLiteral("untitled.pie") : QFileInfo(path).fileName());
    updateTitle();
}

void MainWindow::addTab(const QString& path, const QString& text) {
    auto* editor = new ScriptEditor;
    editor->setPlainText(text);
    int i = tabs_->addTab(editor, path.isEmpty() ? QStringLiteral("untitled.pie") : QFileInfo(path).fileName());
    tabs_->tabBar()->setTabData(i, path);
    tabs_->setCurrentIndex(i);
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this] { updateTitle(); });
    editor->setReadOnly(engine_.isRunning());
    updateTitle();
}

void MainWindow::updateTitle() {
    auto* ed = currentEditor();
    QString name = currentPath().isEmpty() ? QStringLiteral("untitled.pie") : QFileInfo(currentPath()).fileName();
    if (ed && ed->document()->isModified()) name += QLatin1Char('*');
    setWindowTitle(name + QStringLiteral(" — DarkPIE"));
}

void MainWindow::newFile() { addTab({}, {}); }

bool MainWindow::openPath(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("DarkPIE"), QStringLiteral("Could not open file."));
        return false;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    const QString abs = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < tabs_->count(); ++i) {
        if (QFileInfo(tabs_->tabBar()->tabData(i).toString()).absoluteFilePath() == abs) {
            tabs_->setCurrentIndex(i);
            rememberRecent(abs);
            noteScriptDirectory(abs);
            return true;
        }
    }
    addTab(abs, in.readAll());
    rememberRecent(abs);
    noteScriptDirectory(abs);
    return true;
}

void MainWindow::openFile() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open GlovePIE script"), scriptDialogDirectory(),
        QStringLiteral("GlovePIE scripts (*.pie);;Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty()) return;
    openPath(path);
}

bool MainWindow::maybeSave() {
    auto* ed = currentEditor();
    if (!ed || !ed->document()->isModified()) return true;
    auto r = QMessageBox::question(this, QStringLiteral("DarkPIE"), QStringLiteral("Save changes?"),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) saveFile();
    return true;
}

void MainWindow::saveFile() {
    if (currentPath().isEmpty()) {
        saveFileAs();
        return;
    }
    auto* ed = currentEditor();
    if (!ed) return;
    QFile f(currentPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("DarkPIE"), QStringLiteral("Could not save file."));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << ed->toPlainText();
    ed->document()->setModified(false);
    updateTitle();
    rememberRecent(currentPath());
    noteScriptDirectory(currentPath());
}

void MainWindow::saveFileAs() {
    const QString suggested = currentPath().isEmpty()
                                  ? QDir(scriptDialogDirectory()).filePath(QStringLiteral("untitled.pie"))
                                  : currentPath();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save script"), suggested,
        QStringLiteral("GlovePIE scripts (*.pie);;Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty()) return;
    setCurrentPath(path);
    saveFile();
    rememberRecent(QFileInfo(path).absoluteFilePath());
}

void MainWindow::runScript() {
    auto* ed = currentEditor();
    if (!ed) return;
    debug_->clear();
    ed->clearErrors();
    const QString source = ed->toPlainText();
    // Mark problems the same way Check for Errors does, then still start the script.
    Lexer lexer(source.toStdString());
    auto tokens = lexer.tokenize();
    QList<ScriptErrorMark> marks;
    for (const auto& t : tokens) {
        if (t.kind == TokenKind::Unknown) {
            marks.push_back({t.line, t.column, std::max(1, static_cast<int>(t.text.size()))});
        }
    }
    Parser parser(tokens);
    Program program = parser.parseAll();
    for (const auto& err : program.errors)
        marks.push_back({err.line, err.column, std::max(1, err.length)});
    for (const auto& err : checkScriptNames(program))
        marks.push_back({err.line, err.column, std::max(1, err.length)});
    if (!marks.isEmpty()) ed->setErrorMarks(marks);
    engine_.run(source);
    updateRunningUi();
}

void MainWindow::stopScript() {
    engine_.stop();
    updateRunningUi();
}

void MainWindow::toggleRunStop() {
    if (engine_.isRunning()) stopScript();
    else runScript();
}

void MainWindow::onEngineStatus() {
    // Remember the level to put back, so a crash cannot leave the microphone changed.
    QSettings settings;
    auto& speech = engine_.bus().speech();
    if (speech.captureVolumeHeld()) {
        settings.setValue(QStringLiteral("micVolumeHold"), true);
        settings.setValue(QStringLiteral("micVolumeHoldEndpoint"),
                          QString::fromStdString(speech.captureVolumeEndpoint()));
        settings.setValue(QStringLiteral("micVolumeHoldLevel"), speech.captureVolumeOriginal());
    } else if (settings.value(QStringLiteral("micVolumeHold")).toBool()) {
        settings.remove(QStringLiteral("micVolumeHold"));
        settings.remove(QStringLiteral("micVolumeHoldEndpoint"));
        settings.remove(QStringLiteral("micVolumeHoldLevel"));
    }
    QString extra;
    const QString phrase = engine_.lastPhrase();
    if (!phrase.isEmpty()) extra = QStringLiteral(" · heard \"%1\"").arg(phrase);
    statusLabel_->setText(engine_.status() + extra);
    updateRunningUi();
}

void MainWindow::updateRunningUi() {
    const bool running = engine_.isRunning();
    if (runAct_) {
        runAct_->setEnabled(!running);
        runAct_->setToolTip(running ? QStringLiteral("Script is already running")
                                    : QStringLiteral("Run the script"));
    }
    if (stopAct_) {
        stopAct_->setEnabled(running);
        stopAct_->setToolTip(running ? QStringLiteral("Stop the script")
                                     : QStringLiteral("No script is running"));
    }
    if (runStopBtn_) {
        QSignalBlocker block(runStopBtn_);
        runStopBtn_->setChecked(running);
        runStopBtn_->setText(running ? QStringLiteral("Stop") : QStringLiteral("Run"));
        runStopBtn_->setToolTip(running ? QStringLiteral("Stop the script")
                                        : QStringLiteral("Run the script"));
    }
    for (int i = 0; i < tabs_->count(); ++i) {
        if (auto* ed = qobject_cast<ScriptEditor*>(tabs_->widget(i))) {
            ed->setReadOnly(running);
            ed->setPlaceholderText(running ? QStringLiteral("Stop the script to edit") : QString());
        }
    }
}

QStringList MainWindow::loadRecent() const {
    return QSettings().value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::saveRecent(const QStringList& files) const {
    QSettings().setValue(QStringLiteral("recentFiles"), files);
}

QString MainWindow::scriptDialogDirectory() const {
    QSettings settings;
    QString dir = settings.value(QStringLiteral("lastScriptDir")).toString();
    if (dir.isEmpty()) {
        const QStringList recent = loadRecent();
        if (!recent.isEmpty()) dir = QFileInfo(recent.front()).absolutePath();
    }
    if (!dir.isEmpty() && QDir(dir).exists()) return dir;
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::noteScriptDirectory(const QString& filePath) {
    const QString dir = QFileInfo(filePath).absolutePath();
    if (dir.isEmpty() || !QDir(dir).exists()) return;
    QSettings().setValue(QStringLiteral("lastScriptDir"), dir);
}

void MainWindow::rememberRecent(const QString& path) {
    if (path.isEmpty()) return;
    const QString abs = QFileInfo(path).absoluteFilePath();
    QStringList files = loadRecent();
    files.removeAll(abs);
    files.prepend(abs);
    while (files.size() > 12) files.removeLast();
    saveRecent(files);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    if (!recentMenu_) return;
    recentMenu_->clear();
    const QStringList files = loadRecent();
    int shown = 0;
    for (const QString& path : files) {
        if (!QFileInfo::exists(path)) continue;
        auto* act = recentMenu_->addAction(QFileInfo(path).fileName());
        act->setToolTip(path);
        act->setData(path);
        connect(act, &QAction::triggered, this, &MainWindow::openRecent);
        ++shown;
    }
    if (shown == 0) {
        auto* empty = recentMenu_->addAction(QStringLiteral("(No recent scripts)"));
        empty->setEnabled(false);
    } else {
        recentMenu_->addSeparator();
        recentMenu_->addAction(QStringLiteral("&Clear Recent Scripts"), this, &MainWindow::clearRecent);
    }
}

void MainWindow::openRecent() {
    auto* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    const QString path = act->data().toString();
    if (!maybeSave()) return;
    if (!openPath(path)) {
        QStringList files = loadRecent();
        files.removeAll(path);
        saveRecent(files);
        rebuildRecentMenu();
    }
}

void MainWindow::clearRecent() {
    saveRecent({});
    rebuildRecentMenu();
}

void MainWindow::rebuildMicrophoneMenu() {
    if (!micMenu_ || !micGroup_) return;
    const QList<QAction*> old = micGroup_->actions();
    for (auto* a : old) {
        micMenu_->removeAction(a);
        micGroup_->removeAction(a);
        delete a;
    }
    micMenu_->clear();

    const QString current = QString::fromStdString(engine_.bus().speech().microphoneId());
    const auto mics = SpeechDevice::listMicrophones();

    auto addMic = [&](const QString& label, const QString& id) {
        auto* act = micMenu_->addAction(label);
        act->setCheckable(true);
        act->setData(id);
        micGroup_->addAction(act);
        return act;
    };

    addMic(QStringLiteral("&Default microphone"), {})->setChecked(current.isEmpty());

    if (!mics.empty()) {
        micMenu_->addSeparator();
        for (const auto& mic : mics) {
            auto* act = addMic(QString::fromStdString(mic.name), QString::fromStdString(mic.id));
            act->setToolTip(QString::fromStdString(mic.id));
            if (!current.isEmpty() && current == act->data().toString()) act->setChecked(true);
        }
    } else {
        auto* none = micMenu_->addAction(QStringLiteral("No microphones found"));
        none->setEnabled(false);
    }
}

void MainWindow::onMicrophoneChosen(QAction* action) {
    if (!action) return;
    const QString id = action->data().toString();
    engine_.bus().speech().setMicrophoneId(id.toStdString());
    QSettings().setValue(QStringLiteral("microphoneId"), id);
    if (engine_.isRunning()) {
        auto* ed = currentEditor();
        if (ed) {
            debug_->appendPlainText(QStringLiteral("Switching microphone to %1").arg(
                action->text().remove(QLatin1Char('&'))));
            engine_.run(ed->toPlainText());
        }
    }
    updateRunningUi();
}

void MainWindow::showFindReplace() {
    if (!findBar_) return;
    findBar_->setVisible(true);
    if (auto* ed = currentEditor()) {
        QString sel = ed->textCursor().selectedText();
        if (!sel.isEmpty() && !sel.contains(QChar::ParagraphSeparator) && findEdit_ &&
            findEdit_->text().isEmpty()) {
            sel.replace(QChar::LineSeparator, QLatin1Char('\n'));
            findEdit_->setText(sel);
        }
    }
    if (findEdit_) {
        findEdit_->setFocus();
        findEdit_->selectAll();
    }
}

bool MainWindow::findInEditor(bool forward) {
    auto* ed = currentEditor();
    if (!ed || !findEdit_) return false;
    const QString needle = findEdit_->text();
    if (needle.isEmpty()) return false;
    QTextDocument::FindFlags flags;
    if (!forward) flags |= QTextDocument::FindBackward;
    if (matchCaseCheck_ && matchCaseCheck_->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (ed->find(needle, flags)) return true;
    QTextCursor wrap = ed->textCursor();
    wrap.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
    ed->setTextCursor(wrap);
    if (ed->find(needle, flags)) return true;
    if (statusLabel_) statusLabel_->setText(QStringLiteral("Not found: %1").arg(needle));
    return false;
}

void MainWindow::findNext() {
    if (findBar_ && !findBar_->isVisible()) showFindReplace();
    findInEditor(true);
}

void MainWindow::findPrevious() {
    if (findBar_ && !findBar_->isVisible()) showFindReplace();
    findInEditor(false);
}

void MainWindow::replaceOne() {
    auto* ed = currentEditor();
    if (!ed || !findEdit_ || ed->isReadOnly()) return;
    if (findBar_ && !findBar_->isVisible()) showFindReplace();
    const QString needle = findEdit_->text();
    if (needle.isEmpty()) return;
    QString sel = ed->textCursor().selectedText();
    sel.replace(QChar::LineSeparator, QLatin1Char('\n'));
    const Qt::CaseSensitivity cs =
        (matchCaseCheck_ && matchCaseCheck_->isChecked()) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (!sel.isEmpty() && QString::compare(sel, needle, cs) == 0) {
        ed->textCursor().insertText(replaceEdit_ ? replaceEdit_->text() : QString());
    }
    findInEditor(true);
}

void MainWindow::replaceAll() {
    auto* ed = currentEditor();
    if (!ed || !findEdit_ || ed->isReadOnly()) return;
    const QString needle = findEdit_->text();
    if (needle.isEmpty()) return;
    const QString repl = replaceEdit_ ? replaceEdit_->text() : QString();
    QTextDocument::FindFlags flags;
    if (matchCaseCheck_ && matchCaseCheck_->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    QTextCursor cur(ed->document());
    cur.beginEditBlock();
    int n = 0;
    int pos = 0;
    while (true) {
        QTextCursor found = ed->document()->find(needle, pos, flags);
        if (found.isNull()) break;
        found.insertText(repl);
        pos = found.position();
        ++n;
    }
    cur.endEditBlock();
    if (statusLabel_)
        statusLabel_->setText(QStringLiteral("Replaced %1 occurrence(s)").arg(n));
}

void MainWindow::checkScript() {
    auto* ed = currentEditor();
    if (!ed) return;
    ed->clearErrors();
    const QString source = ed->toPlainText();
    Lexer lexer(source.toStdString());
    auto tokens = lexer.tokenize();
    QList<ScriptErrorMark> marks;
    QStringList report;
    for (const auto& t : tokens) {
        if (t.kind == TokenKind::Unknown) {
            marks.push_back({t.line, t.column, std::max(1, static_cast<int>(t.text.size()))});
            report << QStringLiteral("Line %1: unexpected character '%2'")
                        .arg(t.line)
                        .arg(QString::fromStdString(t.text));
        }
    }
    Parser parser(std::move(tokens));
    Program program = parser.parseAll();
    for (const auto& err : program.errors) {
        marks.push_back({err.line, err.column, std::max(1, err.length)});
        report << QStringLiteral("Line %1: %2")
                    .arg(err.line)
                    .arg(QString::fromStdString(err.message));
    }
    for (const auto& err : checkScriptNames(program)) {
        marks.push_back({err.line, err.column, std::max(1, err.length)});
        report << QStringLiteral("Line %1: %2")
                    .arg(err.line)
                    .arg(QString::fromStdString(err.message));
    }
    debug_->appendPlainText(QStringLiteral("—— Script check ——"));
    if (report.isEmpty()) {
        debug_->appendPlainText(QStringLiteral("No errors found."));
        if (statusLabel_) statusLabel_->setText(QStringLiteral("No errors found."));
        QMessageBox::information(this, QStringLiteral("Check for Errors"),
                                 QStringLiteral("No errors found."));
        return;
    }
    ed->setErrorMarks(marks);
    for (const QString& line : report) debug_->appendPlainText(line);
    if (statusLabel_)
        statusLabel_->setText(QStringLiteral("Line: %1 · %2 error(s)")
                                  .arg(marks.front().line)
                                  .arg(report.size()));
    QMessageBox::warning(this, QStringLiteral("Check for Errors"),
                         QStringLiteral("%1 error(s) found. The caret is on the first one.\n\n"
                                        "You can still press Run — good lines will still work.\n\n%2")
                             .arg(report.size())
                             .arg(report.join(QLatin1Char('\n'))));
}

void MainWindow::toggleSleepWake() {
    const bool on = sleepWakeAct_ && sleepWakeAct_->isChecked();
    engine_.bus().speech().setSleepWakeEnabled(on);
    QSettings().setValue(QStringLiteral("sleepWakeEnabled"), on);
    debug_->appendPlainText(on ? QStringLiteral("Sleep / wake commands enabled. Say \"go to sleep\" or \"wake up\".")
                               : QStringLiteral("Sleep / wake commands disabled."));
    if (engine_.isRunning()) {
        auto* ed = currentEditor();
        if (ed) engine_.run(ed->toPlainText());
    }
    updateRunningUi();
}

void MainWindow::onThemeChosen(QAction* action) {
    if (!action) return;
    const auto id = static_cast<DarkPieThemeId>(action->data().toInt());
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        applyDarkPieTheme(*app, id);
    }
    saveDarkPieThemeId(id);
    applyThemeToEditors();
}

void MainWindow::applyThemeToEditors() {
    if (!tabs_) return;
    for (int i = 0; i < tabs_->count(); ++i) {
        if (auto* ed = qobject_cast<ScriptEditor*>(tabs_->widget(i)))
            ed->applyThemeColors();
    }
}

void MainWindow::openSpeechTraining() {
    if (launchSpeechWizard(QStringLiteral("UserTraining"))) {
        debug_->appendPlainText(QStringLiteral("Opened Windows Speech Recognition training."));
        return;
    }
    if (launchDetached(QStringLiteral("control.exe"), {QStringLiteral("/name"), QStringLiteral("Microsoft.SpeechRecognition")})) {
        debug_->appendPlainText(QStringLiteral("Opened Speech Recognition in Control Panel."));
        return;
    }
    openSpeechSettings();
}

void MainWindow::openMicrophoneSetup() {
    if (launchSpeechWizard(QStringLiteral("MicTraining"))) {
        debug_->appendPlainText(QStringLiteral("Opened Windows microphone setup."));
        return;
    }
    openSpeechSettings();
}

void MainWindow::openSpeechSettings() {
    if (launchDetached(QStringLiteral("explorer.exe"), {QStringLiteral("ms-settings:speech")})) {
        debug_->appendPlainText(QStringLiteral("Opened Windows Speech settings."));
        return;
    }
    QMessageBox::warning(this, QStringLiteral("DarkPIE"),
                         QStringLiteral("Could not open Windows speech training. Open Settings → Time & language → Speech."));
}
