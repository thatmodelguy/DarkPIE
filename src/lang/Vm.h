#pragma once

#include "Ast.h"
#include "Value.h"

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class DeviceBus;

class Vm {
public:
    void load(Program program);
    void reset();
    void tick(DeviceBus& bus);

    bool hasError() const { return !error_.empty(); }
    const std::string& error() const { return error_; }
    int errorLine() const { return errorLine_; }

    std::vector<std::string> collectSaidPhrases() const;
    void setNow(std::chrono::steady_clock::time_point t) { now_ = t; useFakeNow_ = true; }

private:
    enum class Flow { Next, SkipRest };

    Value eval(const Expr& expr, DeviceBus& bus);
    Flow exec(const Stmt& stmt, DeviceBus& bus, bool inIf);
    Flow execList(const std::vector<StmtPtr>& list, DeviceBus& bus, bool inIf);
    std::string targetPath(const Expr& expr, DeviceBus& bus);
    void assign(const Expr& target, const Value& value, DeviceBus& bus, bool force);
    void collectSaid(const Expr* expr, std::vector<std::string>& out) const;
    void collectSaidStmt(const Stmt* stmt, std::vector<std::string>& out) const;
    std::chrono::steady_clock::time_point now() const;

    Program program_;
    std::string error_;
    int errorLine_ = 0;
    std::unordered_map<int, Value> lastAssigned_;
    std::unordered_map<int, size_t> seqPc_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> waitUntil_;
    std::unordered_map<int, bool> seqActive_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> keepDownUntil_;
    std::unordered_map<std::string, bool> toggles_;
    std::unordered_map<int, double> deltaPrev_;
    std::unordered_map<int, bool> deltaHasPrev_;
    std::unordered_map<int, std::vector<double>> smoothHist_;
    std::unordered_map<int, bool> edgePrev_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> heldSince_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> lastClickAt_;
    std::chrono::steady_clock::time_point now_{};
    bool useFakeNow_ = false;
    DeviceBus* bus_ = nullptr;
};
