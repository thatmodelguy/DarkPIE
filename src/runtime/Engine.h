#pragma once

#include "devices/DeviceBus.h"
#include "lang/Parser.h"
#include "lang/Vm.h"

#include <QObject>
#include <QString>
#include <QTimer>

class QWinEventNotifier;

#include <string>

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(QObject* parent = nullptr);

    bool isRunning() const { return running_; }
    QString status() const;
    QString lastPhrase() const;
    DeviceBus& bus() { return bus_; }

public slots:
    bool run(const QString& source);
    void stop();

signals:
    void debugMessage(const QString& text);
    void errorMessage(const QString& text);
    void statusChanged();
    void hidePieRequested();
    void showPieRequested();
    void exitPieRequested();
    void minimizePieRequested();
    void maximizePieRequested();
    void restorePieRequested();
    void flashPieRequested();

private slots:
    void tick();

private:
    void armSpeechNotify();
    void disarmSpeechNotify();

    DeviceBus bus_;
    Vm vm_;
    QTimer timer_;
    QWinEventNotifier* speechNotify_ = nullptr;
    bool running_ = false;
    bool inTick_ = false;
    QString speechStatus_;
    size_t warningCursor_ = 0;
    std::string lastEmittedPhrase_;
};
