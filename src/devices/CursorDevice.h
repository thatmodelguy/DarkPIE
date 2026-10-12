#pragma once

#include "lang/Value.h"

#include <string>
#include <unordered_map>

class CursorDevice {
public:
    CursorDevice();
    ~CursorDevice();

    CursorDevice(const CursorDevice&) = delete;
    CursorDevice& operator=(const CursorDevice&) = delete;

    bool dryRun = false;

    Value get(int index, const std::string& name);
    void set(int index, const std::string& name, const Value& value);
    void reset();

private:
    struct Slot {
        int index = 0;
        void* hwnd = nullptr;
        bool created = false;
        bool visible = true;
        int x = 0;
        int y = 0;
        bool left = false;
        bool right = false;
        bool middle = false;
    };

    Slot& slot(int index);
    void ensureWindow(Slot& s);
    void updateWindow(Slot& s);
    void destroyWindow(Slot& s);
    void sendClick(int x, int y, const char* button, bool down);

    std::unordered_map<int, Slot> slots_;
};
