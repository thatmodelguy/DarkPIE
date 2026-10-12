#pragma once

#include "lang/Value.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class KeyboardDevice {
public:
    KeyboardDevice();
    ~KeyboardDevice();

    bool dryRun = false;

    Value get(const std::string& keyName);
    void setHeld(const std::string& keyName, bool down);
    void setProperty(const std::string& name, const Value& value);
    void press(const std::string& keyName);
    void release(const std::string& keyName);
    void releaseAllHeld();
    void typeText(const std::string& text);
    void swallow(const std::string& keyName, bool on);
    bool isSwallowedVk(unsigned long vk) const;
    void endFrame();
    void reset();

    static std::string canonicalize(std::string name);
    static bool isKnownKey(const std::string& name);

    const std::unordered_set<std::string>& heldThisFrame() const { return desiredHeld_; }

private:
    unsigned short vkFor(const std::string& name) const;
    void sendVk(unsigned short vk, bool down);
    void sendNamed(const std::string& key, bool down);
    void sendUnicode(wchar_t ch, bool down);
    int keyboardCount() const;
    bool anyKeyDown() const;
    void installSwallowHook();
    void removeSwallowHook();

    std::unordered_map<std::string, unsigned short> map_;
    std::unordered_set<std::string> desiredHeld_;
    std::unordered_set<std::string> physicallyHeld_;
    std::unordered_set<unsigned short> swallowedVk_;
    std::vector<std::pair<unsigned short, bool>> taps_;
    bool repeatFakeKeys_ = true;
    bool repeatMultipleFakeKeys_ = true;
    double repeatRateHz_ = 0;
    double repeatDelaySec_ = 0.4;
    void* swallowHook_ = nullptr;
};
