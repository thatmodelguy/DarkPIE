#include "Engine.h"

#include "devices/SpeechDevice.h"
#include "lang/Lexer.h"

#include <Windows.h>

namespace {
bool processIsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) == TRUE;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}
}

Engine::Engine(QObject* parent) : QObject(parent) {
    timer_.setInterval(16);
    connect(&timer_, &QTimer::timeout, this, &Engine::tick);
    bus_.setDebugHandler([this](const std::string& text) { emit debugMessage(QString::fromStdString(text)); });
}

QString Engine::status() const {
    if (!running_) return QStringLiteral("Stopped");
    return QStringLiteral("Running · %1").arg(speechStatus_);
}

QString Engine::lastPhrase() const {
    return QString::fromStdString(bus_.speech().lastRecognized);
}

bool Engine::run(const QString& source) {
    stop();
    Lexer lexer(source.toStdString());
    Parser parser(lexer.tokenize());
    Program program = parser.parse();
    if (!program.error.empty()) {
        emit errorMessage(QStringLiteral("Parse error (line %1): %2")
                              .arg(program.errorLine)
                              .arg(QString::fromStdString(program.error)));
        return false;
    }
    vm_.load(std::move(program));
    bus_.reset();
    warningCursor_ = 0;
    std::string speechError;
    auto phrases = vm_.collectSaidPhrases();
    if (!bus_.speech().start(phrases, speechError)) {
        speechStatus_ = QString::fromStdString(speechError);
        emit errorMessage(speechStatus_);
    } else {
        speechStatus_ = QString::fromStdString(bus_.speech().status());
    }
    running_ = true;
    timer_.start();
    emit statusChanged();
    emit debugMessage(QStringLiteral("Script started. %1").arg(speechStatus_));
    if (!processIsElevated()) {
        emit errorMessage(QStringLiteral(
            "DarkPIE is not running as administrator. Fullscreen games often ignore keys until you "
            "restart DarkPIE elevated (same as GlovePIE)."));
    }
    return true;
}

void Engine::stop() {
    timer_.stop();
    if (running_) {
        running_ = false;
        bus_.speech().stop();
        vm_.reset();
        bus_.reset();
        emit debugMessage(QStringLiteral("Script stopped."));
        emit statusChanged();
    }
}

void Engine::tick() {
    if (!running_) return;
    bus_.beginFrame();
    if (bus_.speech().consumeWentToSleep()) {
        bus_.releaseGeneratedOutput();
        bus_.speech().say("Sleeping");
        speechStatus_ = QString::fromStdString(bus_.speech().status());
        emit debugMessage(QStringLiteral("Voice commands sleeping. Say \"wake up\" to resume."));
        emit statusChanged();
    }
    if (bus_.speech().consumeWokeUp()) {
        bus_.speech().say("Ready");
        speechStatus_ = QString::fromStdString(bus_.speech().status());
        emit debugMessage(QStringLiteral("Voice commands awake."));
        emit statusChanged();
    }
    vm_.tick(bus_);
    bus_.endFrame();
    if (bus_.hideRequested) {
        bus_.hideRequested = false;
        emit hidePieRequested();
    }
    if (bus_.showRequested) {
        bus_.showRequested = false;
        emit showPieRequested();
    }
    if (bus_.minimizeRequested) {
        bus_.minimizeRequested = false;
        emit minimizePieRequested();
    }
    if (bus_.maximizeRequested) {
        bus_.maximizeRequested = false;
        emit maximizePieRequested();
    }
    if (bus_.restoreRequested) {
        bus_.restoreRequested = false;
        emit restorePieRequested();
    }
    if (bus_.flashRequested) {
        bus_.flashRequested = false;
        emit flashPieRequested();
    }
    if (bus_.exitRequested) {
        bus_.exitRequested = false;
        stop();
        emit exitPieRequested();
        return;
    }
    if (vm_.hasError()) {
        emit errorMessage(QStringLiteral("Runtime error (line %1): %2")
                              .arg(vm_.errorLine())
                              .arg(QString::fromStdString(vm_.error())));
    }
    const auto& warnings = bus_.warnings();
    while (warningCursor_ < warnings.size()) {
        emit errorMessage(QString::fromStdString(warnings[warningCursor_]));
        ++warningCursor_;
    }
    const auto progress = bus_.speech().takeProgress();
    for (const auto& note : progress) emit debugMessage(QString::fromStdString(note));
    if (bus_.speech().lastRecognized != lastEmittedPhrase_) {
        lastEmittedPhrase_ = bus_.speech().lastRecognized;
        if (!lastEmittedPhrase_.empty() && progress.empty()) {
            const bool sleeping = bus_.speech().sleeping();
            const bool builtin = lastEmittedPhrase_ == "go to sleep" || lastEmittedPhrase_ == "wake up";
            if (sleeping && !builtin)
                emit debugMessage(QStringLiteral("Ignored while sleeping: ") +
                                  QString::fromStdString(lastEmittedPhrase_));
            else
                emit debugMessage(QStringLiteral("Heard: ") + QString::fromStdString(lastEmittedPhrase_));
        }
        emit statusChanged();
    }
}
