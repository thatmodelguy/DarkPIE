#pragma once

#include "lang/Value.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class MouseDevice {
public:
    MouseDevice();
    ~MouseDevice();

    MouseDevice(const MouseDevice&) = delete;
    MouseDevice& operator=(const MouseDevice&) = delete;

    bool dryRun = false;

    Value get(const std::string& name);
    Value getIndexed(int index, const std::string& name);
    void set(const std::string& name, const Value& value);
    void poll();
    void endFrame();
    void releaseAllButtons();
    void reset();

    static std::string canonicalize(std::string name);

    int fakeDirectX() const { return fakeDirectX_; }
    int fakeDirectY() const { return fakeDirectY_; }
    int realDirectX() const { return combinedRealX_; }
    int realDirectY() const { return combinedRealY_; }

    void addRawDelta(int dx, int dy);
    void handleRaw(void* headerDevice, int dx, int dy, unsigned short buttonFlags);

private:
    struct Phys {
        void* handle = nullptr;
        bool present = false;
        int x = 0;
        int y = 0;
        std::unordered_set<std::string> buttons;
    };

    void sendButton(const std::string& name, bool down);
    void sendWheel(int clicks, bool horizontal);
    void setCursorPixel(int x, int y);
    void moveRelative(int dx, int dy);
    bool isButton(const std::string& name) const;
    void ensureRawInput();
    void pumpRawInput();
    void refreshPhysicalMice();
    Phys& phys(int index);
    void setSwallow(bool on);
    static std::string buttonFromFlags(unsigned short flags, bool& down, bool& known);

    std::unordered_set<std::string> desiredHeld_;
    std::unordered_set<std::string> physicallyHeld_;
    int fakeDirectX_ = 0;
    int fakeDirectY_ = 0;
    int combinedRealX_ = 0;
    int combinedRealY_ = 0;
    int pendingRawX_ = 0;
    int pendingRawY_ = 0;
    int stickyX_ = 0;
    int stickyY_ = 0;
    long long lastMoveMs_ = 0;
    void* hwnd_ = nullptr;
    void* swallowHook_ = nullptr;
    bool rawReady_ = false;
    bool swallow_ = false;
    std::unordered_map<void*, int> handleIndex_;
    std::vector<Phys> mice_; // 1-based in usage, stored 0-based
};
