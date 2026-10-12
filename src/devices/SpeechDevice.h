#pragma once

#include "lang/Value.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ISpVoice;
struct ISpRecognizer;
struct ISpRecoContext;
struct ISpRecoGrammar;

struct MicrophoneInfo {
    std::string id;
    std::string name;
};

class SpeechDevice {
public:
    SpeechDevice();
    ~SpeechDevice();

    SpeechDevice(const SpeechDevice&) = delete;
    SpeechDevice& operator=(const SpeechDevice&) = delete;

    static std::vector<MicrophoneInfo> listMicrophones();
    void setMicrophoneId(std::string id);
    const std::string& microphoneId() const { return microphoneId_; }

    bool start(const std::vector<std::string>& phrases, std::string& error);
    void stop();
    void poll();

    bool said(const std::string& phrase, int confidence);
    void injectRecognition(const std::string& phrase, int sapiConfidence, bool partial);
    void say(const std::string& text);
    void setSpeechText(const std::string& text);

    void setSleepWakeEnabled(bool enabled);
    bool sleepWakeEnabled() const { return sleepWakeEnabled_; }
    bool sleeping() const;
    bool consumeWentToSleep();
    bool consumeWokeUp();

    bool microphoneEnabled = true;
    bool pushToNotTalk = false;
    bool inPhrase = false;
    bool inSound = false;
    std::string completePhrase;
    std::string partialPhrase;
    std::string lastRecognized;
    bool ready() const { return ready_; }
    std::string status() const;

    Value getProperty(const std::string& object, const std::string& name);
    void setProperty(const std::string& object, const std::string& name, const Value& value);

private:
    std::string normalize(std::string s) const;
    bool confidenceOk(int required, int sapiConfidence, bool partial) const;

    std::string microphoneId_;

    ISpVoice* voice_ = nullptr;
    ISpRecognizer* recognizer_ = nullptr;
    ISpRecoContext* context_ = nullptr;
    ISpRecoGrammar* grammar_ = nullptr;
    bool comInit_ = false;
    bool ready_ = false;
    std::string status_ = "Speech idle";
    std::string lastSpeechText_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, int> pulses_;      // phrase -> max confidence this frame
    std::unordered_map<std::string, int> hypothese_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastPulseAt_;
    std::unordered_set<std::string> knownPhrases_;

    bool isKnownPhrase(const std::string& heard) const;
    bool tryPulse(const std::string& phrase, int conf);
    void ingestRecoText(const std::string& phrase, int conf, bool hypothesis);
    void pulseMatches(const std::string& heard, int conf, bool strict);
    void applySleepWake();
    bool heardSleepCommandLocked() const;
    bool heardWakeCommandLocked() const;

    bool sleepWakeEnabled_ = true;
    bool asleep_ = false;
    bool wentToSleep_ = false;
    bool wokeUp_ = false;
};
