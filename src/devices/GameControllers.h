#pragma once

#include "lang/Value.h"

#include <string>
#include <vector>

class GameControllers {
public:
    GameControllers();
    ~GameControllers();

    GameControllers(const GameControllers&) = delete;
    GameControllers& operator=(const GameControllers&) = delete;

    bool dryRun = false;

    bool handles(const std::string& root);
    Value get(const std::string& root, const std::string& rest);
    void set(const std::string& root, const std::string& rest, const Value& value);
    void poll();
    void reset();
    void pokeWiimote(int index, int address, int value);
    void sendWiimote(int index, int report, const std::vector<int>& bytes);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
