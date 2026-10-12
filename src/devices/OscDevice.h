#pragma once

#include "lang/Value.h"

#include <string>
#include <vector>

class OscDevice {
public:
    OscDevice();
    ~OscDevice();

    bool dryRun = false;

    bool handles(const std::string& root) const;
    Value get(const std::string& root, const std::string& rest);
    void set(const std::string& root, const std::string& rest, const Value& value);
    void send(const std::string& ip, int port, const std::string& address, const std::vector<Value>& args);
    void poll();
    void reset();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
