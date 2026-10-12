#pragma once

#include <cmath>
#include <string>
#include <vector>

enum class ValueKind { Null, Bool, Number, String, Vector };

struct Value {
    ValueKind kind = ValueKind::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<double> vec;
    std::string unit;

    static Value null() { return {}; }
    static Value boolean(bool v) {
        Value r;
        r.kind = ValueKind::Bool;
        r.b = v;
        return r;
    }
    static Value number(double v, std::string u = {}) {
        Value r;
        r.kind = ValueKind::Number;
        r.n = v;
        r.unit = std::move(u);
        return r;
    }
    static Value string(std::string v) {
        Value r;
        r.kind = ValueKind::String;
        r.s = std::move(v);
        return r;
    }
    static Value vector(std::vector<double> v, std::string u = {}) {
        Value r;
        r.kind = ValueKind::Vector;
        r.vec = std::move(v);
        r.unit = std::move(u);
        return r;
    }

    std::vector<double> asVector() const {
        if (kind == ValueKind::Vector) return vec;
        if (kind == ValueKind::Number) return {n};
        if (kind == ValueKind::Bool) return {b ? 1.0 : 0.0};
        return {};
    }

    bool isTruthy() const {
        switch (kind) {
        case ValueKind::Bool: return b;
        // GlovePIE: true if greater than 0.5 or less than -0.5 (so analog triggers work as buttons).
        case ValueKind::Number: return n > 0.5 || n < -0.5;
        case ValueKind::String: return !s.empty();
        case ValueKind::Vector:
            for (double x : vec)
                if (x != 0) return true;
            return false;
        default: return false;
        }
    }

    double asNumber() const {
        switch (kind) {
        case ValueKind::Number: return n;
        case ValueKind::Bool: return b ? 1.0 : 0.0;
        case ValueKind::String:
            try {
                return std::stod(s);
            } catch (...) {
                return 0;
            }
        default: return 0;
        }
    }

    std::string asString() const {
        switch (kind) {
        case ValueKind::String: return s;
        case ValueKind::Bool: return b ? "true" : "false";
        case ValueKind::Number: {
            if (std::floor(n) == n) return std::to_string(static_cast<long long>(n));
            return std::to_string(n);
        }
        case ValueKind::Vector: {
            std::string out = "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i) out += ", ";
                if (std::floor(vec[i]) == vec[i]) out += std::to_string(static_cast<long long>(vec[i]));
                else out += std::to_string(vec[i]);
            }
            out += "]";
            return out;
        }
        default: return "";
        }
    }

    bool equals(const Value& o) const {
        if (kind == ValueKind::Vector || o.kind == ValueKind::Vector) {
            auto a = asVector();
            auto b = o.asVector();
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i] != b[i]) return false;
            return true;
        }
        if (kind == ValueKind::String || o.kind == ValueKind::String)
            return asString() == o.asString();
        if (kind == ValueKind::Bool && o.kind == ValueKind::Bool) return b == o.b;
        return asNumber() == o.asNumber();
    }
};
