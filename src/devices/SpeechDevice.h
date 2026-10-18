#pragma once

#include "lang/Value.h"

#include <mutex>
#include <string>
#include <unordered_map>
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

    // While a script is listening, the selected microphone's Windows volume is
    // set to this percent and the previous level is restored when listening stops.
    void setCaptureVolumePercent(int percent);
    int captureVolumePercent() const { return capturePercent_; }
    bool captureVolumeHeld() const { return volumeHeld_; }
    const std::string& captureVolumeEndpoint() const { return volumeEndpoint_; }
    float captureVolumeOriginal() const { return volumeOriginal_; }
    static bool forceEndpointVolume(const std::string& endpointId, float scalar);

    bool start(const std::vector<std::string>& phrases, std::string& error);
    void stop();
    void poll();
    // Manual-reset event signaled when SAPI has a guess or a final result.
    // The script host waits on this so a voice command is not stuck behind the frame timer.
    void* notifyEvent() const;

    bool said(const std::string& phrase, int confidence);
    void injectRecognition(const std::string& phrase, int sapiConfidence, bool partial);
    // Script phrases only — lets tests exercise guessing without a microphone.
    void setCommandsForTest(const std::vector<std::string>& phrases);
    void hearForTest(const std::string& phrase, int confidence, bool hypothesis, unsigned long long utteranceId);
    std::vector<std::string> takeProgress();
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
    std::wstring activeTokenId_;
    int capturePercent_ = 100;
    bool volumeHeld_ = false;
    std::string volumeEndpoint_;
    float volumeOriginal_ = 1.f;

    void applyCaptureVolume();
    void restoreCaptureVolume();

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

    struct CommandPhrase {
        std::string display;
        std::vector<std::string> words;
    };
    struct PendingPulse {
        std::string phrase;
        int conf = 0;
    };
    std::vector<CommandPhrase> commands_;
    std::vector<PendingPulse> pendingPulses_;
    std::vector<std::string> progress_;
    // How many times each command has already fired inside the current utterance.
    std::unordered_map<std::string, int> firedThisUtterance_;
    unsigned long long utterancePos_ = 0;
    bool expectNewUtterance_ = true;
    bool utteranceClosed_ = false;

    void rebuildCommands(const std::vector<std::string>& phrases);
    bool extendsLonger(const CommandPhrase& cmd) const;
    std::vector<std::string> matchCommands(const std::string& heard, bool hypothesis) const;
    void beginResultUtterance(unsigned long long audioPos);
    void queueMatches(const std::string& heard, int conf, bool hypothesis);
    void emitPendingPulse();
    void ingestRecoText(const std::string& phrase, int conf, bool hypothesis, unsigned long long audioPos);
    void applySleepWake();
    bool heardSleepCommandLocked() const;
    bool heardWakeCommandLocked() const;

    bool sleepWakeEnabled_ = true;
    bool asleep_ = false;
    bool wentToSleep_ = false;
    bool wokeUp_ = false;
};
