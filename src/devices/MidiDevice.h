#pragma once

#include "lang/Value.h"

#include <string>

class MidiDevice {
public:
    MidiDevice();
    ~MidiDevice();

    MidiDevice(const MidiDevice&) = delete;
    MidiDevice& operator=(const MidiDevice&) = delete;

    bool dryRun = false;

    bool handles(const std::string& root) const;
    Value get(const std::string& root, const std::string& rest);
    void set(const std::string& root, const std::string& rest, const Value& value);
    void reset();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
