#include "ScriptCheck.h"

#include "devices/KeyboardDevice.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int editDistance(const std::string& a, const std::string& b) {
    const size_t n = a.size(), m = b.size();
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = static_cast<int>(i);
        for (size_t j = 1; j <= m; ++j) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }
    return prev[m];
}

std::string suggest(const std::string& got, const std::vector<std::string>& candidates) {
    const std::string g = lower(got);
    std::string best;
    int bestDist = 1000;
    for (const auto& c : candidates) {
        const int d = editDistance(g, lower(c));
        if (d < bestDist) {
            bestDist = d;
            best = c;
        }
    }
    if (best.empty()) return {};
    const int limit = std::max(1, static_cast<int>(std::ceil(std::max(g.size(), best.size()) / 3.0)));
    if (bestDist == 0 || bestDist > std::max(2, limit)) return {};
    return best;
}

const std::vector<std::string>& knownFunctions() {
    static const std::vector<std::string> fns = {
        "Said", "AskedFor", "KeepDown", "Say", "Press", "Release", "Type", "Toggle",
        "Inc", "Dec", "Delta", "Smooth", "Pressed", "Clicked", "SingleClicked", "Released",
        "DoubleClicked", "HeldDown", "Swallow", "MapRange", "EnsureMapRange", "EnsureRange",
        "InRange", "Deadzone", "Abs", "Min", "Max", "Sign", "Sqrt", "Sqr", "Power", "IntPower",
        "Sin", "Cos", "Tan", "ASin", "ACos", "ATan", "ATan2", "ArcTan2", "Sinh", "Cosh", "Tanh",
        "ArcSin", "Exp", "Ln", "Log10", "Log2", "LogN", "IsZero", "IsNaN", "IsInfinite", "Odd",
        "Pred", "Succ", "TimeStamp", "GetTimeStamp", "Now", "Date", "Time", "SameValue", "InSet",
        "Floor", "Ceil", "Round", "Trunc", "Int", "Frac", "Random", "RandomRange", "Beep",
        "Execute", "Chain", "DebugPrint", "Display", "ScreenPixel", "IntToHex", "Kalman",
        "RoundTo", "SimpleRoundTo", "TypeUnicode", "PlaySound",
        "HidePie", "ShowPie", "ExitPie", "ExitProgram", "FlashPieWindow", "MinimizePie",
        "MaximizePie", "RestorePie", "UnMinimizePie", "UnMaximizePie", "AddCode",
        "SendOsc", "BroadcastOsc", "WiimotePoke", "WiimoteSend",
        "BeepHand", "BeepQuestion", "BeepDefault", "BeepAsterisk", "BeepExclamation",
        "ControlPanel", "ControlPanelJoystick", "ControlPanelSpeech", "ControlPanelKeyboard",
        "ControlPanelMouse", "ControlPanelMidi", "ControlPanelPPJoy"};
    return fns;
}

const std::vector<std::string>& knownDevices() {
    static const std::vector<std::string> devices = {
        "Key", "Keyboard", "Mouse", "FakeMouse", "RealMouse", "Cursor", "Screen", "Window",
        "Touchpad", "Speech", "Microphone", "Midi", "Pie", "var", "debug", "Joystick", "Joy",
        "Wiimote", "Nunchuk", "Classic", "Guitar", "Sixaxis", "XInput", "PPJoy", "OSC", "P5",
        "Glove", "Kinect", "TrackIR", "Falcon", "Spaceball", "Rower", "BDRemote"};
    return devices;
}

const std::vector<std::string>& micMethods() {
    static const std::vector<std::string> methods = {
        "Said", "AskedFor", "Say", "CompletePhrase", "PartialPhrase", "Enabled", "InPhrase",
        "InSound", "Confidence", "AudioLevel", "PushToTalk", "PushToNotTalk", "HeardNoise",
        "NoSignal", "TooLoud", "TooQuiet", "TooFast", "TooSlow"};
    return methods;
}

bool isKnownFunction(const std::string& name) {
    const std::string n = lower(name);
    for (const auto& f : knownFunctions())
        if (lower(f) == n) return true;
    return false;
}

bool isDeviceRoot(const std::string& rootRaw) {
    std::string root = lower(rootRaw);
    if (root.empty()) return false;
    for (const auto& dRaw : knownDevices()) {
        const std::string d = lower(dRaw);
        if (root == d) return true;
        if (root.size() > d.size() && root.compare(0, d.size(), d) == 0 &&
            std::isdigit(static_cast<unsigned char>(root[d.size()])))
            return true;
    }
    return false;
}

bool isMicObject(const Expr* expr) {
    if (auto* name = dynamic_cast<const NameExpr*>(expr)) {
        const std::string n = lower(name->name);
        return n == "microphone" || n == "speech";
    }
    return false;
}

ParseError makeError(int line, int column, const std::string& spell, const std::string& message) {
    return {line, column, std::max(1, static_cast<int>(spell.size())), message};
}

void checkExpr(const Expr* expr, std::vector<ParseError>& out);
void checkStmt(const Stmt* stmt, std::vector<ParseError>& out);

void checkCall(const CallExpr* call, std::vector<ParseError>& out) {
    if (!call) return;
    if (call->object) checkExpr(call->object.get(), out);
    for (const auto& a : call->args) checkExpr(a.get(), out);

    if (call->object) {
        if (isMicObject(call->object.get())) {
            const std::string n = lower(call->name);
            bool ok = false;
            for (const auto& m : micMethods())
                if (lower(m) == n) {
                    ok = true;
                    break;
                }
            if (!ok) {
                std::string msg = "Unknown microphone command \"" + call->name + "\"";
                const std::string hint = suggest(call->name, micMethods());
                if (!hint.empty()) msg += " (did you mean " + hint + "?)";
                out.push_back(makeError(call->line, call->column, call->name, msg));
            }
            return;
        }
        // object.method() for other devices: only flag if the object root looks wrong
        if (auto* name = dynamic_cast<const NameExpr*>(call->object.get())) {
            if (!isDeviceRoot(name->name) && !KeyboardDevice::isKnownKey(lower(name->name))) {
                std::string msg = "Unknown device \"" + name->name + "\"";
                const std::string hint = suggest(name->name, knownDevices());
                if (!hint.empty()) msg += " (did you mean " + hint + "?)";
                out.push_back(makeError(name->line, name->column, name->name, msg));
            }
        }
        return;
    }

    if (!isKnownFunction(call->name)) {
        std::string msg = "Unknown function \"" + call->name + "\"";
        const std::string hint = suggest(call->name, knownFunctions());
        if (!hint.empty()) msg += " (did you mean " + hint + "?)";
        out.push_back(makeError(call->line, call->column, call->name, msg));
    }
}

void checkPathExpr(const Expr* expr, std::vector<ParseError>& out, bool asTarget) {
    if (!expr) return;
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        // toggle(key.x) = ...
        if (lower(call->name) == "toggle") {
            for (const auto& a : call->args) checkPathExpr(a.get(), out, true);
            return;
        }
        checkCall(call, out);
        return;
    }
    if (auto* mem = dynamic_cast<const MemberExpr*>(expr)) {
        checkExpr(mem->object.get(), out);
        if (auto* name = dynamic_cast<const NameExpr*>(mem->object.get())) {
            if (!isDeviceRoot(name->name)) {
                std::string msg = "Unknown device \"" + name->name + "\"";
                const std::string hint = suggest(name->name, knownDevices());
                if (!hint.empty()) msg += " (did you mean " + hint + "?)";
                out.push_back(makeError(name->line, name->column, name->name, msg));
            } else if (isMicObject(mem->object.get())) {
                const std::string n = lower(mem->name);
                bool ok = false;
                for (const auto& m : micMethods())
                    if (lower(m) == n) {
                        ok = true;
                        break;
                    }
                if (!ok) {
                    std::string msg = "Unknown microphone property \"" + mem->name + "\"";
                    const std::string hint = suggest(mem->name, micMethods());
                    if (!hint.empty()) msg += " (did you mean " + hint + "?)";
                    out.push_back(makeError(mem->line, mem->column, mem->name, msg));
                }
            }
        }
        return;
    }
    if (auto* name = dynamic_cast<const NameExpr*>(expr)) {
        const std::string n = lower(name->name);
        if (n == "debug" || n == "starting") return;
        if (isDeviceRoot(n)) return;
        if (KeyboardDevice::isKnownKey(n)) return;
        // Bare unknown names as assignment targets are often typos for Key.*
        if (asTarget) {
            std::string msg = "Unknown name \"" + name->name + "\"";
            const std::string hint = suggest(name->name, knownDevices());
            if (!hint.empty()) msg += " (did you mean " + hint + "?)";
            else msg += " (key names are Key." + name->name + " or just " + name->name + " if it is a key)";
            // Only flag if it looks like a device typo; bare made-up vars are uncommon without var.
            if (!hint.empty()) out.push_back(makeError(name->line, name->column, name->name, msg));
        }
        return;
    }
    checkExpr(expr, out);
}

void checkExpr(const Expr* expr, std::vector<ParseError>& out) {
    if (!expr) return;
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        checkCall(call, out);
        return;
    }
    if (auto* mem = dynamic_cast<const MemberExpr*>(expr)) {
        checkExpr(mem->object.get(), out);
        return;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        checkExpr(un->inner.get(), out);
        return;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        checkExpr(bin->left.get(), out);
        checkExpr(bin->right.get(), out);
        return;
    }
    if (auto* bet = dynamic_cast<const BetweenExpr*>(expr)) {
        checkExpr(bet->low.get(), out);
        checkExpr(bet->mid.get(), out);
        checkExpr(bet->high.get(), out);
        return;
    }
    if (auto* arr = dynamic_cast<const ArrayExpr*>(expr)) {
        for (const auto& e : arr->elements) checkExpr(e.get(), out);
        return;
    }
}

void checkStmt(const Stmt* stmt, std::vector<ParseError>& out) {
    if (!stmt) return;
    if (auto* as = dynamic_cast<const AssignStmt*>(stmt)) {
        checkPathExpr(as->target.get(), out, true);
        checkExpr(as->value.get(), out);
        return;
    }
    if (auto* es = dynamic_cast<const ExprStmt*>(stmt)) {
        checkExpr(es->expr.get(), out);
        return;
    }
    if (auto* block = dynamic_cast<const BlockStmt*>(stmt)) {
        for (const auto& s : block->statements) checkStmt(s.get(), out);
        return;
    }
    if (auto* ifs = dynamic_cast<const IfStmt*>(stmt)) {
        checkExpr(ifs->condition.get(), out);
        for (const auto& s : ifs->thenBody) checkStmt(s.get(), out);
        for (const auto& s : ifs->elseBody) checkStmt(s.get(), out);
        return;
    }
    if (auto* wait = dynamic_cast<const WaitStmt*>(stmt)) {
        checkExpr(wait->duration.get(), out);
        return;
    }
    if (auto* inc = dynamic_cast<const IncrementStmt*>(stmt)) {
        checkPathExpr(inc->target.get(), out, true);
        return;
    }
    if (auto* wh = dynamic_cast<const WhileStmt*>(stmt)) {
        checkExpr(wh->condition.get(), out);
        for (const auto& s : wh->body) checkStmt(s.get(), out);
        return;
    }
    if (auto* fr = dynamic_cast<const ForStmt*>(stmt)) {
        checkStmt(fr->init.get(), out);
        checkExpr(fr->condition.get(), out);
        checkStmt(fr->increment.get(), out);
        for (const auto& s : fr->body) checkStmt(s.get(), out);
        return;
    }
}

}  // namespace

std::vector<ParseError> checkScriptNames(const Program& program) {
    std::vector<ParseError> out;
    for (const auto& s : program.statements) checkStmt(s.get(), out);
    return out;
}
