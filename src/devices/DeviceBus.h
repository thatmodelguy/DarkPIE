#pragma once

#include "lang/Value.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class KeyboardDevice;
class SpeechDevice;
class MouseDevice;
class CursorDevice;
class GameControllers;
class MidiDevice;
class OscDevice;

class DeviceBus {
public:
    DeviceBus();
    ~DeviceBus();

    DeviceBus(const DeviceBus&) = delete;
    DeviceBus& operator=(const DeviceBus&) = delete;

    KeyboardDevice& keyboard() { return *keyboard_; }
    const KeyboardDevice& keyboard() const { return *keyboard_; }
    SpeechDevice& speech() { return *speech_; }
    const SpeechDevice& speech() const { return *speech_; }
    MouseDevice& mouse() { return *mouse_; }
    const MouseDevice& mouse() const { return *mouse_; }

    Value get(const std::string& path);
    void set(const std::string& path, const Value& value, bool force);

    void setDebugHandler(std::function<void(const std::string&)> handler);
    std::string consumeDebug();
    std::string lastError() const { return lastError_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

    void beginFrame();
    void endFrame();
    void releaseGeneratedOutput();
    void reset();

    void setVar(const std::string& name, const Value& value);
    Value getVar(const std::string& name) const;

    GameControllers& controllers() { return *controllers_; }

    void sendOsc(const std::string& ip, int port, const std::string& address, const std::vector<Value>& args);
    void warn(const std::string& key, const std::string& message);

    bool dryRun = false;
    bool starting = false;
    bool exitRequested = false;
    bool hideRequested = false;
    bool showRequested = false;
    bool minimizeRequested = false;
    bool maximizeRequested = false;
    bool restoreRequested = false;
    bool flashRequested = false;
    double mouseDpi = 400;
    double pieEpsilon = 1e-4;

private:
    void warnOnce(const std::string& key, const std::string& message);
    Value screenGet(const std::string& name) const;
    void screenSet(const std::string& name, const Value& value);
    Value windowGet(const std::string& name) const;
    Value touchpadGet(const std::string& name);
    void touchpadSet(const std::string& name, const Value& value);
    Value oscGet(int index, const std::string& rest);
    void oscSet(int index, const std::string& rest, const Value& value);

    KeyboardDevice* keyboard_ = nullptr;
    SpeechDevice* speech_ = nullptr;
    MouseDevice* mouse_ = nullptr;
    CursorDevice* cursors_ = nullptr;
    GameControllers* controllers_ = nullptr;
    MidiDevice* midi_ = nullptr;
    OscDevice* osc_ = nullptr;
    std::unordered_map<std::string, Value> vars_;
    std::function<void(const std::string&)> debugHandler_;
    std::string debugText_;
    std::string lastError_;
    std::vector<std::string> warnings_;
    std::unordered_map<std::string, bool> warned_;
    int frames_ = 0;
};
