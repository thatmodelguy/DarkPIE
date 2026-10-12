#include "Vm.h"

#include "devices/DeviceBus.h"
#include "devices/GameControllers.h"
#include "devices/KeyboardDevice.h"
#include "devices/MouseDevice.h"
#include "devices/SpeechDevice.h"

#include <Windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <random>

namespace {
Value combineValues(const Value& l, const Value& r, const std::string& op) {
    const bool vec = l.kind == ValueKind::Vector || r.kind == ValueKind::Vector;
    if (vec) {
        auto a = l.asVector();
        auto b = r.asVector();
        if (op == "+" || op == "-") {
            const size_t n = std::max(a.size(), b.size());
            a.resize(n, 0);
            b.resize(n, 0);
            std::vector<double> out(n);
            for (size_t i = 0; i < n; ++i) out[i] = (op == "+") ? a[i] + b[i] : a[i] - b[i];
            auto v = Value::vector(std::move(out), l.unit.empty() ? r.unit : l.unit);
            return v;
        }
        if (op == "*") {
            if (l.kind == ValueKind::Vector && r.kind != ValueKind::Vector) {
                double s = r.asNumber();
                for (double& x : a) x *= s;
                return Value::vector(std::move(a), l.unit);
            }
            if (r.kind == ValueKind::Vector && l.kind != ValueKind::Vector) {
                double s = l.asNumber();
                for (double& x : b) x *= s;
                return Value::vector(std::move(b), r.unit);
            }
            const size_t n = std::min(a.size(), b.size());
            std::vector<double> out(n);
            for (size_t i = 0; i < n; ++i) out[i] = a[i] * b[i];
            return Value::vector(std::move(out), l.unit);
        }
        if (op == "/") {
            std::string u = l.unit;
            if (r.unit == "s" && u.find('/') == std::string::npos)
                u = (u.empty() ? "mickeys" : u) + "/s";
            if (r.kind != ValueKind::Vector) {
                double s = r.asNumber();
                if (s == 0) return Value::vector(std::vector<double>(a.size(), 0), u);
                for (double& x : a) x /= s;
                return Value::vector(std::move(a), u);
            }
            const size_t n = std::min(a.size(), b.size());
            std::vector<double> out(n);
            for (size_t i = 0; i < n; ++i) out[i] = b[i] == 0 ? 0 : a[i] / b[i];
            return Value::vector(std::move(out), u);
        }
    }
    auto tagged = [&](double n) {
        std::string u = l.unit.empty() ? r.unit : l.unit;
        if (op == "/" && (r.unit == "s") && u.find('/') == std::string::npos)
            u = (u.empty() ? "mickeys" : u) + "/s";
        return Value::number(n, u);
    };
    if (op == "+") {
        if (l.kind == ValueKind::String || r.kind == ValueKind::String)
            return Value::string(l.asString() + r.asString());
        return tagged(l.asNumber() + r.asNumber());
    }
    if (op == "-") return tagged(l.asNumber() - r.asNumber());
    if (op == "*") return tagged(l.asNumber() * r.asNumber());
    if (op == "/") {
        double d = r.asNumber();
        return tagged(d == 0 ? 0 : l.asNumber() / d);
    }
    return Value::null();
}

Value convertUnit(Value v, const std::string& target, double dpi) {
    if (target.empty()) return v;
    auto conv = [&](double n) {
        const std::string& from = v.unit;
        if (from == target) return n;
        if (target == "in" && (from == "mickeys" || from.empty())) return n / dpi;
        if (target == "mickeys" && from == "in") return n * dpi;
        if (target == "px" && (from == "mickeys" || from.empty() || from == "px")) return n;
        if (target == "px" && from == "in") return n * dpi;
        if (target == "in" && from == "px") return n / dpi;
        if (target == "in/s" && (from == "mickeys/s" || from == "mickeys" || from.empty())) return n / dpi;
        if (target == "in/s" && from == "px/s") return n / dpi;
        if (target == "px/s" && (from == "mickeys/s" || from == "mickeys" || from.empty())) return n;
        return n;
    };
    if (v.kind == ValueKind::Vector) {
        for (double& x : v.vec) x = conv(x);
        v.unit = target;
        return v;
    }
    return Value::number(conv(v.asNumber()), target);
}
}

void Vm::load(Program program) {
    program_ = std::move(program);
    reset();
}

void Vm::reset() {
    error_.clear();
    errorLine_ = 0;
    lastAssigned_.clear();
    seqPc_.clear();
    waitUntil_.clear();
    seqActive_.clear();
    keepDownUntil_.clear();
    toggles_.clear();
    deltaPrev_.clear();
    deltaHasPrev_.clear();
    smoothHist_.clear();
    edgePrev_.clear();
    heldSince_.clear();
    lastClickAt_.clear();
}

std::chrono::steady_clock::time_point Vm::now() const {
    if (useFakeNow_) return now_;
    return std::chrono::steady_clock::now();
}

std::string Vm::targetPath(const Expr& expr, DeviceBus& bus) {
    if (auto* name = dynamic_cast<const NameExpr*>(&expr)) return name->name;
    if (auto* mem = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string obj = targetPath(*mem->object, bus);
        if (obj.empty()) return mem->name;
        return obj + "." + mem->name;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr); bin && (bin->op == "+" || bin->op == "and")) {
        return targetPath(*bin->left, bus) + "+" + targetPath(*bin->right, bus);
    }
    return eval(expr, bus).asString();
}

Value Vm::eval(const Expr& expr, DeviceBus& bus) {
    if (auto* lit = dynamic_cast<const LiteralExpr*>(&expr)) return lit->value;
    if (auto* name = dynamic_cast<const NameExpr*>(&expr)) return bus.get(name->name);
    if (auto* mem = dynamic_cast<const MemberExpr*>(&expr)) {
        return bus.get(targetPath(*mem, bus));
    }
    if (auto* bet = dynamic_cast<const BetweenExpr*>(&expr)) {
        Value lo = eval(*bet->low, bus);
        Value mid = eval(*bet->mid, bus);
        Value hi = eval(*bet->high, bus);
        auto cmp = [](const std::string& op, double a, double b) {
            if (op == "<") return a < b;
            if (op == ">") return a > b;
            if (op == "<=") return a <= b;
            if (op == ">=") return a >= b;
            return false;
        };
        return Value::boolean(cmp(bet->opLo, lo.asNumber(), mid.asNumber()) &&
                              cmp(bet->opHi, mid.asNumber(), hi.asNumber()));
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        Value inner = eval(*un->inner, bus);
        if (un->op == "not" || un->op == "!") return Value::boolean(!inner.isTruthy());
        if (un->op == "-") {
            if (inner.kind == ValueKind::Vector) {
                auto v = inner.vec;
                for (double& x : v) x = -x;
                return Value::vector(std::move(v), inner.unit);
            }
            return Value::number(-inner.asNumber(), inner.unit);
        }
        if (un->op == "%") return Value::number(inner.asNumber() / 100.0, inner.unit);
        if (un->op == "abs") {
            if (inner.kind == ValueKind::Vector) {
                double s = 0;
                for (double x : inner.vec) s += x * x;
                return Value::number(std::sqrt(s), inner.unit);
            }
            return Value::number(std::fabs(inner.asNumber()), inner.unit);
        }
        if (un->op.rfind("in:", 0) == 0) {
            double dpi = bus.get("pie.mousedpi").asNumber();
            if (dpi <= 0) dpi = 400;
            return convertUnit(inner, un->op.substr(3), dpi);
        }
        if (un->op.rfind("as:", 0) == 0) {
            inner.unit = un->op.substr(3);
            return inner;
        }
        return inner;
    }
    if (auto* arr = dynamic_cast<const ArrayExpr*>(&expr)) {
        std::vector<double> xs;
        xs.reserve(arr->elements.size());
        for (const auto& e : arr->elements) xs.push_back(e ? eval(*e, bus).asNumber() : 0);
        return Value::vector(std::move(xs));
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (bin->op == "or") {
            Value l = eval(*bin->left, bus);
            if (l.isTruthy()) return Value::boolean(true);
            return Value::boolean(eval(*bin->right, bus).isTruthy());
        }
        if (bin->op == "and") {
            Value l = eval(*bin->left, bus);
            if (!l.isTruthy()) return Value::boolean(false);
            return Value::boolean(eval(*bin->right, bus).isTruthy());
        }
        if (bin->op == "xor") {
            bool l = eval(*bin->left, bus).isTruthy();
            bool r = eval(*bin->right, bus).isTruthy();
            return Value::boolean(l != r);
        }
        if (bin->op == "neither") {
            return Value::boolean(!eval(*bin->left, bus).isTruthy() && !eval(*bin->right, bus).isTruthy());
        }
        Value l = eval(*bin->left, bus);
        Value r = eval(*bin->right, bus);
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/")
            return combineValues(l, r, bin->op);
        if (bin->op == "%of") return Value::number(l.asNumber() / 100.0 * r.asNumber());
        if (bin->op == "mod") {
            double d = r.asNumber();
            return Value::number(d == 0 ? 0 : std::fmod(l.asNumber(), d), l.unit);
        }
        if (bin->op == "div") {
            double d = r.asNumber();
            return Value::number(d == 0 ? 0 : std::trunc(l.asNumber() / d), l.unit);
        }
        if (bin->op == "&")
            return Value::number(static_cast<double>(static_cast<long long>(l.asNumber()) &
                                                     static_cast<long long>(r.asNumber())));
        if (bin->op == "^") return Value::number(std::pow(l.asNumber(), r.asNumber()), l.unit);
        if (bin->op == "shl")
            return Value::number(l.asNumber() * std::pow(2.0, r.asNumber()), l.unit);
        if (bin->op == "shr")
            return Value::number(std::trunc(l.asNumber() / std::pow(2.0, r.asNumber())), l.unit);
        if (bin->op == "~=") {
            if (l.kind == ValueKind::String || r.kind == ValueKind::String) {
                auto trimLower = [](std::string s) {
                    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                    while (!s.empty() && s.back() == ' ') s.pop_back();
                    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return s;
                };
                return Value::boolean(trimLower(l.asString()) == trimLower(r.asString()));
            }
            double eps = bus.get("pie.epsilon").asNumber();
            if (eps <= 0) eps = 1e-4;
            return Value::boolean(std::fabs(l.asNumber() - r.asNumber()) <= eps);
        }
        if (bin->op == "=" || bin->op == "==") return Value::boolean(l.equals(r));
        if (bin->op == "<>" || bin->op == "!=") return Value::boolean(!l.equals(r));
        if (bin->op == "<") return Value::boolean(l.asNumber() < r.asNumber());
        if (bin->op == ">") return Value::boolean(l.asNumber() > r.asNumber());
        if (bin->op == "<=") return Value::boolean(l.asNumber() <= r.asNumber());
        if (bin->op == ">=") return Value::boolean(l.asNumber() >= r.asNumber());
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        std::string fn = call->name;
        auto arg = [&](size_t i) -> Value {
            if (i >= call->args.size()) return Value::null();
            return eval(*call->args[i], bus);
        };
        if (fn == "said" || fn == "microphone.said") {
            int conf = call->args.size() >= 2 ? static_cast<int>(arg(1).asNumber()) : 2;
            return Value::boolean(bus.speech().said(arg(0).asString(), conf));
        }
        if (fn == "keepdown") {
            bool x = !call->args.empty() && eval(*call->args[0], bus).isTruthy();
            double seconds = call->args.size() >= 2 ? arg(1).asNumber() : 0;
            auto t = now();
            if (x) {
                keepDownUntil_[call->id] = t + std::chrono::milliseconds(static_cast<int>(seconds * 1000.0));
                return Value::boolean(true);
            }
            auto it = keepDownUntil_.find(call->id);
            if (it != keepDownUntil_.end() && t < it->second) return Value::boolean(true);
            return Value::boolean(false);
        }
        if (fn == "askedfor") {
            int conf = call->args.size() >= 2 ? static_cast<int>(arg(1).asNumber()) : 2;
            return Value::boolean(bus.speech().said(arg(0).asString(), conf));
        }
        if (fn == "say") {
            bus.speech().say(arg(0).asString());
            return Value::boolean(true);
        }
        if (fn == "press" || fn == "release") {
            std::string path = call->args.empty() ? std::string() : targetPath(*call->args[0], bus);
            for (char& c : path) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool down = fn == "press";
            auto dot = path.find('.');
            std::string root = dot == std::string::npos ? path : path.substr(0, dot);
            std::string rest = dot == std::string::npos ? std::string() : path.substr(dot + 1);
            if (root == "mouse" || root == "fakemouse" || root == "realmouse") {
                bus.mouse().set(rest, Value::boolean(down));
            } else {
                bus.keyboard().setHeld(rest.empty() ? root : rest, down);
            }
            return Value::boolean(true);
        }
        if (fn == "type") {
            bus.keyboard().typeText(arg(0).asString());
            return Value::boolean(true);
        }
        if (fn == "inc" || fn == "dec") {
            if (!call->args.empty()) {
                std::string path = targetPath(*call->args[0], bus);
                double n = bus.get(path).asNumber() + (fn == "dec" ? -1 : 1);
                bus.set(path, Value::number(n), true);
            }
            return Value::boolean(true);
        }
        if (fn == "delta") {
            Value xv = arg(0);
            double x = xv.asNumber();
            if (!deltaHasPrev_[call->id]) {
                deltaHasPrev_[call->id] = true;
                deltaPrev_[call->id] = x;
                return Value::number(0, xv.unit);
            }
            double d = x - deltaPrev_[call->id];
            deltaPrev_[call->id] = x;
            return Value::number(d, xv.unit);
        }
        if (fn == "smooth") {
            Value xv = arg(0);
            double x = xv.asNumber();
            int extra = call->args.size() >= 2 ? std::max(1, static_cast<int>(arg(1).asNumber())) : 5;
            auto& hist = smoothHist_[call->id];
            hist.push_back(x);
            while (static_cast<int>(hist.size()) > extra + 1) hist.erase(hist.begin());
            double sum = 0;
            for (double v : hist) sum += v;
            return Value::number(sum / static_cast<double>(hist.size()), xv.unit);
        }
        if (fn == "pressed" || fn == "clicked" || fn == "singleclicked") {
            bool cur = !call->args.empty() && eval(*call->args[0], bus).isTruthy();
            bool prev = edgePrev_[call->id];
            edgePrev_[call->id] = cur;
            return Value::boolean(cur && !prev);
        }
        if (fn == "released") {
            bool cur = !call->args.empty() && eval(*call->args[0], bus).isTruthy();
            bool prev = edgePrev_[call->id];
            edgePrev_[call->id] = cur;
            return Value::boolean(!cur && prev);
        }
        if (fn == "screenpixel") {
            int x = static_cast<int>(arg(0).asNumber());
            int y = static_cast<int>(arg(1).asNumber());
            HDC dc = GetDC(nullptr);
            COLORREF c = dc ? GetPixel(dc, x, y) : 0;
            if (dc) ReleaseDC(nullptr, dc);
            int rgb = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
            return Value::number(rgb);
        }
        if (fn == "inttohex") {
            long long n = static_cast<long long>(arg(0).asNumber());
            int digits = call->args.size() >= 2 ? static_cast<int>(arg(1).asNumber()) : 0;
            char buf[32];
            if (digits > 0) std::snprintf(buf, sizeof(buf), "%0*llX", digits, n);
            else std::snprintf(buf, sizeof(buf), "%llX", n);
            return Value::string(buf);
        }
        if (fn == "kalman") return arg(0);
        if (fn == "roundto" || fn == "simpleroundto") {
            double x = arg(0).asNumber();
            double digits = call->args.size() >= 2 ? arg(1).asNumber() : -2;
            double scale = std::pow(10.0, -digits);
            return Value::number(std::round(x / scale) * scale, arg(0).unit);
        }
        if (fn == "doubleclicked") {
            bool cur = !call->args.empty() && eval(*call->args[0], bus).isTruthy();
            bool prev = edgePrev_[call->id];
            edgePrev_[call->id] = cur;
            if (cur && !prev) {
                auto t = now();
                auto it = lastClickAt_.find(call->id);
                double window = bus.mouse().get("doubleclicktime").asNumber();
                if (window <= 0) window = 0.5;
                if (it != lastClickAt_.end() && t - it->second < std::chrono::milliseconds(static_cast<int>(window * 1000))) {
                    lastClickAt_.erase(it);
                    return Value::boolean(true);
                }
                lastClickAt_[call->id] = t;
            }
            return Value::boolean(false);
        }
        if (fn == "helddown") {
            bool x = !call->args.empty() && eval(*call->args[0], bus).isTruthy();
            double seconds = call->args.size() >= 2 ? arg(1).asNumber() : 0;
            auto t = now();
            if (!x) {
                heldSince_.erase(call->id);
                return Value::boolean(false);
            }
            if (!heldSince_.count(call->id)) heldSince_[call->id] = t;
            return Value::boolean(t - heldSince_[call->id] >= std::chrono::milliseconds(static_cast<int>(seconds * 1000)));
        }
        if (fn == "swallow") {
            std::string path = call->args.empty() ? std::string() : targetPath(*call->args[0], bus);
            for (char& c : path) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            auto dot = path.find('.');
            std::string key = dot == std::string::npos ? path : path.substr(dot + 1);
            bus.keyboard().swallow(key, true);
            return bus.keyboard().get(key);
        }
        if (fn == "maprange") {
            double x = arg(0).asNumber(), a = arg(1).asNumber(), b = arg(2).asNumber(), c = arg(3).asNumber(),
                   d = arg(4).asNumber();
            if (b == a) return Value::number(c, arg(0).unit);
            return Value::number(c + (x - a) * (d - c) / (b - a), arg(0).unit);
        }
        if (fn == "ensuremaprange") {
            double x = arg(0).asNumber(), a = arg(1).asNumber(), b = arg(2).asNumber(), c = arg(3).asNumber(),
                   d = arg(4).asNumber();
            if (b == a) return Value::number(c, arg(0).unit);
            double t = (x - a) / (b - a);
            t = std::clamp(t, 0.0, 1.0);
            return Value::number(c + t * (d - c), arg(0).unit);
        }
        if (fn == "ensurerange") {
            double x = arg(0).asNumber(), a = arg(1).asNumber(), b = arg(2).asNumber();
            if (a > b) std::swap(a, b);
            return Value::number(std::clamp(x, a, b), arg(0).unit);
        }
        if (fn == "inrange") {
            double x = arg(0).asNumber(), a = arg(1).asNumber(), b = arg(2).asNumber();
            if (a > b) std::swap(a, b);
            return Value::boolean(x >= a && x <= b);
        }
        if (fn == "deadzone") {
            double x = arg(0).asNumber();
            double a = call->args.size() >= 2 ? arg(1).asNumber() : 0.1;
            if (std::fabs(x) <= a) return Value::number(0, arg(0).unit);
            double s = x < 0 ? -1 : 1;
            return Value::number(s * (std::fabs(x) - a) / (1.0 - a), arg(0).unit);
        }
        if (fn == "abs") {
            Value xv = arg(0);
            if (xv.kind == ValueKind::Vector) {
                double s = 0;
                for (double v : xv.vec) s += v * v;
                return Value::number(std::sqrt(s), xv.unit);
            }
            return Value::number(std::fabs(xv.asNumber()), xv.unit);
        }
        if (fn == "min") return Value::number(std::min(arg(0).asNumber(), arg(1).asNumber()), arg(0).unit);
        if (fn == "max") return Value::number(std::max(arg(0).asNumber(), arg(1).asNumber()), arg(0).unit);
        if (fn == "sign") {
            double x = arg(0).asNumber();
            return Value::number(x < 0 ? -1 : x > 0 ? 1 : 0);
        }
        if (fn == "sqrt") return Value::number(std::sqrt(std::max(0.0, arg(0).asNumber())), arg(0).unit);
        if (fn == "sqr") {
            double x = arg(0).asNumber();
            return Value::number(x * x, arg(0).unit);
        }
        if (fn == "power" || fn == "intpower") return Value::number(std::pow(arg(0).asNumber(), arg(1).asNumber()));
        auto deg2rad = [](double d) { return d * 3.14159265358979323846 / 180.0; };
        auto rad2deg = [](double r) { return r * 180.0 / 3.14159265358979323846; };
        if (fn == "sin") return Value::number(std::sin(deg2rad(arg(0).asNumber())));
        if (fn == "cos") return Value::number(std::cos(deg2rad(arg(0).asNumber())));
        if (fn == "tan") return Value::number(std::tan(deg2rad(arg(0).asNumber())));
        if (fn == "asin") return Value::number(rad2deg(std::asin(std::clamp(arg(0).asNumber(), -1.0, 1.0))));
        if (fn == "acos") return Value::number(rad2deg(std::acos(std::clamp(arg(0).asNumber(), -1.0, 1.0))));
        if (fn == "atan") return Value::number(rad2deg(std::atan(arg(0).asNumber())));
        if (fn == "atan2" || fn == "arctan2")
            return Value::number(rad2deg(std::atan2(arg(0).asNumber(), arg(1).asNumber())));
        if (fn == "sinh") return Value::number(std::sinh(deg2rad(arg(0).asNumber())));
        if (fn == "cosh") return Value::number(std::cosh(deg2rad(arg(0).asNumber())));
        if (fn == "tanh") return Value::number(std::tanh(deg2rad(arg(0).asNumber())));
        if (fn == "arcsin" || fn == "asin")
            return Value::number(rad2deg(std::asin(std::clamp(arg(0).asNumber(), -1.0, 1.0))));
        if (fn == "exp") return Value::number(std::exp(arg(0).asNumber()), arg(0).unit);
        if (fn == "ln") return Value::number(std::log(std::max(1e-300, arg(0).asNumber())));
        if (fn == "log10") return Value::number(std::log10(std::max(1e-300, arg(0).asNumber())));
        if (fn == "log2") return Value::number(std::log2(std::max(1e-300, arg(0).asNumber())));
        if (fn == "logn")
            return Value::number(std::log(std::max(1e-300, arg(1).asNumber())) /
                                 std::log(std::max(1e-300, arg(0).asNumber())));
        if (fn == "iszero") return Value::boolean(arg(0).asNumber() == 0);
        if (fn == "isnan") return Value::boolean(std::isnan(arg(0).asNumber()));
        if (fn == "isinfinite") return Value::boolean(std::isinf(arg(0).asNumber()));
        if (fn == "odd") return Value::boolean((static_cast<long long>(arg(0).asNumber()) & 1) != 0);
        if (fn == "pred") return Value::number(arg(0).asNumber() - 1, arg(0).unit);
        if (fn == "succ") return Value::number(arg(0).asNumber() + 1, arg(0).unit);
        if (fn == "timestamp" || fn == "gettimestamp") {
            auto t = now().time_since_epoch();
            return Value::number(std::chrono::duration<double>(t).count(), "s");
        }
        if (fn == "now" || fn == "date" || fn == "time") {
            auto t = std::chrono::system_clock::now();
            double ole = std::chrono::duration<double>(t.time_since_epoch()).count() / 86400.0 + 25569.0;
            if (fn == "time") ole = ole - std::floor(ole);
            if (fn == "date") ole = std::floor(ole);
            return Value::number(ole, "d");
        }
        if (fn == "samevalue") {
            double eps = call->args.size() >= 3 ? arg(2).asNumber() : bus.get("pie.epsilon").asNumber();
            if (eps <= 0) eps = 1e-4;
            return Value::boolean(std::fabs(arg(0).asNumber() - arg(1).asNumber()) <= eps);
        }
        if (fn == "inset") {
            double x = arg(0).asNumber();
            for (size_t i = 1; i < call->args.size(); ++i)
                if (arg(i).asNumber() == x) return Value::boolean(true);
            return Value::boolean(false);
        }
        if (fn == "floor") return Value::number(std::floor(arg(0).asNumber()), arg(0).unit);
        if (fn == "ceil") return Value::number(std::ceil(arg(0).asNumber()), arg(0).unit);
        if (fn == "round") return Value::number(std::round(arg(0).asNumber()), arg(0).unit);
        if (fn == "trunc" || fn == "int") return Value::number(std::trunc(arg(0).asNumber()), arg(0).unit);
        if (fn == "frac") {
            double x = arg(0).asNumber();
            return Value::number(x - std::trunc(x), arg(0).unit);
        }
        if (fn == "random") {
            static std::mt19937 rng{std::random_device{}()};
            if (call->args.empty()) {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                return Value::number(dist(rng));
            }
            int n = std::max(1, static_cast<int>(arg(0).asNumber()));
            std::uniform_int_distribution<int> dist(0, n - 1);
            return Value::number(dist(rng));
        }
        if (fn == "randomrange") {
            static std::mt19937 rng{std::random_device{}()};
            int a = static_cast<int>(arg(0).asNumber());
            int b = static_cast<int>(arg(1).asNumber());
            if (a > b) std::swap(a, b);
            std::uniform_int_distribution<int> dist(a, b);
            return Value::number(dist(rng));
        }
        if (fn == "beep") {
            if (call->args.size() >= 2)
                Beep(static_cast<DWORD>(arg(0).asNumber()), static_cast<DWORD>(arg(1).asNumber() * 1000));
            else
                MessageBeep(MB_OK);
            return Value::boolean(true);
        }
        if (fn == "execute" || fn == "chain") {
            std::string cmd = arg(0).asString();
            std::wstring w(cmd.begin(), cmd.end());
            ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "debugprint" || fn == "display") {
            bus.set("debug", arg(0), true);
            return Value::boolean(true);
        }
        if (fn == "controlpanelspeech") {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"/name Microsoft.SpeechRecognition", nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "controlpanelkeyboard") {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"keyboard", nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "controlpanelmouse") {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"mouse", nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "controlpanelmidi") {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"mmsys.cpl", nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "typeunicode") {
            bus.keyboard().typeText(arg(0).asString());
            return Value::boolean(true);
        }
        if (fn == "playsound") {
            std::string f = arg(0).asString();
            std::wstring w(f.begin(), f.end());
            PlaySoundW(w.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
            return Value::boolean(true);
        }
        if (fn == "hidepie") {
            bus.hideRequested = true;
            return Value::boolean(true);
        }
        if (fn == "showpie") {
            bus.showRequested = true;
            return Value::boolean(true);
        }
        if (fn == "exitpie" || fn == "exitprogram") {
            bus.exitRequested = true;
            return Value::boolean(true);
        }
        if (fn == "sendosc" || fn == "broadcastosc") {
            const bool bc = fn == "broadcastosc";
            std::string ip = bc ? std::string("broadcast") : arg(0).asString();
            int port = static_cast<int>(arg(bc ? 0 : 1).asNumber());
            std::string addr = arg(bc ? 1 : 2).asString();
            std::vector<Value> params;
            for (size_t i = bc ? 2 : 3; i < call->args.size(); ++i) params.push_back(arg(i));
            bus.sendOsc(ip, port, addr, params);
            return Value::boolean(true);
        }
        if (fn == "wiimotepoke") {
            int idx = call->args.size() >= 3 ? static_cast<int>(arg(0).asNumber()) : 1;
            int addr = static_cast<int>(arg(call->args.size() >= 3 ? 1 : 0).asNumber());
            int val = static_cast<int>(arg(call->args.size() >= 3 ? 2 : 1).asNumber());
            bus.controllers().pokeWiimote(idx, addr, val);
            return Value::boolean(true);
        }
        if (fn == "wiimotesend") {
            int idx = static_cast<int>(arg(0).asNumber());
            int report = static_cast<int>(arg(1).asNumber());
            std::vector<int> bytes;
            for (size_t i = 2; i < call->args.size(); ++i) bytes.push_back(static_cast<int>(arg(i).asNumber()));
            bus.controllers().sendWiimote(idx, report, bytes);
            return Value::boolean(true);
        }
        if (fn == "beephand") {
            if (!bus.dryRun) MessageBeep(MB_ICONHAND);
            return Value::boolean(true);
        }
        if (fn == "beepquestion") {
            if (!bus.dryRun) MessageBeep(MB_ICONQUESTION);
            return Value::boolean(true);
        }
        if (fn == "beepdefault") {
            if (!bus.dryRun) MessageBeep(MB_OK);
            return Value::boolean(true);
        }
        if (fn == "controlpanel" || fn == "controlpaneljoystick") {
            if (!bus.dryRun) ShellExecuteW(nullptr, L"open", L"control.exe", nullptr, nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        if (fn == "flashpiewindow") {
            bus.flashRequested = true;
            return Value::boolean(true);
        }
        if (fn == "minimizepie") {
            bus.minimizeRequested = true;
            return Value::boolean(true);
        }
        if (fn == "maximizepie") {
            bus.maximizeRequested = true;
            return Value::boolean(true);
        }
        if (fn == "restorepie" || fn == "unminimizepie" || fn == "unmaximizepie") {
            bus.restoreRequested = true;
            return Value::boolean(true);
        }
        if (fn == "addcode") return Value::boolean(true);
        if (fn == "beepasterisk") {
            MessageBeep(MB_ICONASTERISK);
            return Value::boolean(true);
        }
        if (fn == "beepexclamation") {
            MessageBeep(MB_ICONEXCLAMATION);
            return Value::boolean(true);
        }
        if (fn == "controlpanelppjoy") {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"ppjoy.cpl", nullptr, SW_SHOWNORMAL);
            return Value::boolean(true);
        }
        bus.warn("fn:" + fn, "Unknown function '" + fn + "'.");
        return Value::boolean(false);
    }
    return Value::null();
}

void Vm::assign(const Expr& target, const Value& value, DeviceBus& bus, bool force) {
    if (auto* call = dynamic_cast<const CallExpr*>(&target); call && call->name == "toggle" && !call->args.empty()) {
        if (!value.isTruthy()) return;
        std::string path = targetPath(*call->args[0], bus);
        bool next = !toggles_[path];
        toggles_[path] = next;
        bus.set(path, Value::boolean(next), true);
        return;
    }
    bus.set(targetPath(target, bus), value, force);
}

Vm::Flow Vm::execList(const std::vector<StmtPtr>& list, DeviceBus& bus, bool inIf) {
    for (const auto& s : list) {
        if (!s) continue;
        Flow f = exec(*s, bus, inIf);
        if (f == Flow::SkipRest) return Flow::SkipRest;
    }
    return Flow::Next;
}

Vm::Flow Vm::exec(const Stmt& stmt, DeviceBus& bus, bool inIf) {
    if (auto* as = dynamic_cast<const AssignStmt*>(&stmt)) {
        Value v = eval(*as->value, bus);
        const bool compound = as->op == AssignOp::PlusEqual || as->op == AssignOp::MinusEqual ||
                              as->op == AssignOp::StarEqual || as->op == AssignOp::SlashEqual;
        if (compound) {
            Value cur = eval(*as->target, bus);
            std::string op = "+";
            if (as->op == AssignOp::MinusEqual) op = "-";
            else if (as->op == AssignOp::StarEqual) op = "*";
            else if (as->op == AssignOp::SlashEqual) op = "/";
            v = combineValues(cur, v, op);
        }
        bool force = inIf || compound;
        if (!force) {
            auto it = lastAssigned_.find(as->id);
            if (it != lastAssigned_.end() && it->second.equals(v)) return Flow::Next;
            lastAssigned_[as->id] = v;
        }
        assign(*as->target, v, bus, force);
        return Flow::Next;
    }
    if (auto* inc = dynamic_cast<const IncrementStmt*>(&stmt)) {
        std::string path = targetPath(*inc->target, bus);
        bus.set(path, Value::number(bus.get(path).asNumber() + inc->delta), true);
        return Flow::Next;
    }
    if (auto* es = dynamic_cast<const ExprStmt*>(&stmt)) {
        eval(*es->expr, bus);
        return Flow::Next;
    }
    if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        return execList(block->statements, bus, inIf);
    }
    if (auto* wait = dynamic_cast<const WaitStmt*>(&stmt)) {
        double amount = eval(*wait->duration, bus).asNumber();
        std::string unit = wait->unit;
        for (char& c : unit) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        double seconds = amount; // GlovePIE wait is seconds unless a unit is given
        if (unit == "ms" || unit == "millisecond" || unit == "milliseconds") seconds = amount / 1000.0;
        else if (unit == "minute" || unit == "minutes") seconds = amount * 60.0;
        else if (unit == "hour" || unit == "hours") seconds = amount * 3600.0;
        auto dur = std::chrono::milliseconds(static_cast<int>(seconds * 1000.0 + 0.5));
        auto it = waitUntil_.find(wait->id);
        auto t = now();
        if (it == waitUntil_.end()) {
            waitUntil_[wait->id] = t + dur;
            return Flow::SkipRest;
        }
        if (t < it->second) return Flow::SkipRest;
        waitUntil_.erase(it);
        return Flow::Next;
    }
    if (auto* ifs = dynamic_cast<const IfStmt*>(&stmt)) {
        bool active = seqActive_[ifs->id];
        if (!active) {
            if (!eval(*ifs->condition, bus).isTruthy()) {
                if (!ifs->elseBody.empty()) return execList(ifs->elseBody, bus, true);
                return Flow::Next;
            }
            seqActive_[ifs->id] = true;
            seqPc_[ifs->id] = 0;
        }
        auto& body = ifs->thenBody;
        size_t pc = seqPc_[ifs->id];
        while (pc < body.size()) {
            seqPc_[ifs->id] = pc;
            const Stmt& child = *body[pc];
            Flow f = exec(child, bus, true);
            if (dynamic_cast<const WaitStmt*>(&child) && f == Flow::SkipRest) {
                return Flow::Next; // wait in progress; keep sequence alive
            }
            if (f == Flow::SkipRest) {
                seqPc_[ifs->id] = pc;
                return Flow::Next;
            }
            ++pc;
            seqPc_[ifs->id] = pc;
        }
        seqActive_[ifs->id] = false;
        seqPc_.erase(ifs->id);
        return Flow::Next;
    }
    if (auto* wh = dynamic_cast<const WhileStmt*>(&stmt)) {
        auto start = std::chrono::steady_clock::now();
        while (eval(*wh->condition, bus).isTruthy()) {
            execList(wh->body, bus, true);
            if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(200)) {
                bus.speech().say("infinite loop");
                break;
            }
        }
        return Flow::Next;
    }
    if (auto* fr = dynamic_cast<const ForStmt*>(&stmt)) {
        auto start = std::chrono::steady_clock::now();
        auto timedOut = [&] {
            if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(200)) {
                bus.speech().say("infinite loop");
                return true;
            }
            return false;
        };
        if (fr->init) exec(*fr->init, bus, true);
        if (fr->variable && fr->from && fr->to) {
            double a = eval(*fr->from, bus).asNumber();
            double b = eval(*fr->to, bus).asNumber();
            double st = fr->step ? eval(*fr->step, bus).asNumber() : 1;
            if (fr->downto) {
                if (st > 0) st = -st;
            }
            if (st == 0) st = fr->downto ? -1 : 1;
            for (double i = a; fr->downto ? i >= b : i <= b; i += st) {
                assign(*fr->variable, Value::number(i), bus, true);
                execList(fr->body, bus, true);
                if (timedOut()) break;
            }
            return Flow::Next;
        }
        while (!fr->condition || eval(*fr->condition, bus).isTruthy()) {
            execList(fr->body, bus, true);
            if (fr->increment) exec(*fr->increment, bus, true);
            if (timedOut()) break;
            if (!fr->condition) break;
        }
        return Flow::Next;
    }
    return Flow::Next;
}

void Vm::tick(DeviceBus& bus) {
    bus_ = &bus;
    if (!program_.error.empty()) {
        error_ = program_.error;
        errorLine_ = program_.errorLine;
        return;
    }
    execList(program_.statements, bus, false);
}

void Vm::collectSaid(const Expr* expr, std::vector<std::string>& out) const {
    if (!expr) return;
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        if ((call->name == "said" || call->name == "askedfor") && !call->args.empty()) {
            if (auto* lit = dynamic_cast<const LiteralExpr*>(call->args[0].get())) {
                if (lit->value.kind == ValueKind::String) {
                    std::string phrase = lit->value.s;
                    while (!phrase.empty() && std::isspace(static_cast<unsigned char>(phrase.front())))
                        phrase.erase(phrase.begin());
                    while (!phrase.empty() && std::isspace(static_cast<unsigned char>(phrase.back())))
                        phrase.pop_back();
                    if (!phrase.empty()) out.push_back(phrase);
                }
            }
        }
        for (const auto& a : call->args) collectSaid(a.get(), out);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        collectSaid(bin->left.get(), out);
        collectSaid(bin->right.get(), out);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        collectSaid(un->inner.get(), out);
    } else if (auto* bet = dynamic_cast<const BetweenExpr*>(expr)) {
        collectSaid(bet->low.get(), out);
        collectSaid(bet->mid.get(), out);
        collectSaid(bet->high.get(), out);
    } else if (auto* mem = dynamic_cast<const MemberExpr*>(expr)) {
        collectSaid(mem->object.get(), out);
    } else if (auto* arr = dynamic_cast<const ArrayExpr*>(expr)) {
        for (const auto& e : arr->elements) collectSaid(e.get(), out);
    }
}

void Vm::collectSaidStmt(const Stmt* stmt, std::vector<std::string>& out) const {
    if (!stmt) return;
    if (auto* as = dynamic_cast<const AssignStmt*>(stmt)) {
        collectSaid(as->target.get(), out);
        collectSaid(as->value.get(), out);
    } else if (auto* es = dynamic_cast<const ExprStmt*>(stmt)) {
        collectSaid(es->expr.get(), out);
    } else if (auto* block = dynamic_cast<const BlockStmt*>(stmt)) {
        for (const auto& s : block->statements) collectSaidStmt(s.get(), out);
    } else if (auto* ifs = dynamic_cast<const IfStmt*>(stmt)) {
        collectSaid(ifs->condition.get(), out);
        for (const auto& s : ifs->thenBody) collectSaidStmt(s.get(), out);
        for (const auto& s : ifs->elseBody) collectSaidStmt(s.get(), out);
    } else if (auto* wait = dynamic_cast<const WaitStmt*>(stmt)) {
        collectSaid(wait->duration.get(), out);
    } else if (auto* inc = dynamic_cast<const IncrementStmt*>(stmt)) {
        collectSaid(inc->target.get(), out);
    } else if (auto* wh = dynamic_cast<const WhileStmt*>(stmt)) {
        collectSaid(wh->condition.get(), out);
        for (const auto& s : wh->body) collectSaidStmt(s.get(), out);
    } else if (auto* fr = dynamic_cast<const ForStmt*>(stmt)) {
        collectSaidStmt(fr->init.get(), out);
        collectSaid(fr->condition.get(), out);
        collectSaidStmt(fr->increment.get(), out);
        for (const auto& s : fr->body) collectSaidStmt(s.get(), out);
    }
}

std::vector<std::string> Vm::collectSaidPhrases() const {
    std::vector<std::string> out;
    for (const auto& s : program_.statements) collectSaidStmt(s.get(), out);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
