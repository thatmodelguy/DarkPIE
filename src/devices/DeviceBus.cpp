#include "DeviceBus.h"

#include "CursorDevice.h"
#include "GameControllers.h"
#include "KeyboardDevice.h"
#include "MidiDevice.h"
#include "MouseDevice.h"
#include "OscDevice.h"
#include "SpeechDevice.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool isUnsupportedDevice(const std::string& root) {
    static const char* names[] = {"p5", "glove", "kinect", "trackir", "falcon", "spaceball",
                                  "rower", "bdremote"};
    for (auto n : names) {
        if (root == n) return true;
        if (root.rfind(n, 0) == 0 && root.size() > strlen(n) && std::isdigit(static_cast<unsigned char>(root[strlen(n)])))
            return true;
    }
    return false;
}

bool indexedName(const std::string& root, const char* prefix, int& index) {
    const size_t n = std::strlen(prefix);
    if (root.size() <= n || root.compare(0, n, prefix) != 0) return false;
    if (!std::isdigit(static_cast<unsigned char>(root[n]))) return false;
    index = std::stoi(root.substr(n));
    return true;
}
}

DeviceBus::DeviceBus() {
    keyboard_ = new KeyboardDevice();
    speech_ = new SpeechDevice();
    mouse_ = new MouseDevice();
    cursors_ = new CursorDevice();
    controllers_ = new GameControllers();
    midi_ = new MidiDevice();
    osc_ = new OscDevice();
}

DeviceBus::~DeviceBus() {
    delete osc_;
    delete midi_;
    delete controllers_;
    delete cursors_;
    delete mouse_;
    delete speech_;
    delete keyboard_;
}

void DeviceBus::setDebugHandler(std::function<void(const std::string&)> handler) {
    debugHandler_ = std::move(handler);
}

std::string DeviceBus::consumeDebug() {
    std::string t = debugText_;
    return t;
}

void DeviceBus::warn(const std::string& key, const std::string& message) { warnOnce(key, message); }

void DeviceBus::warnOnce(const std::string& key, const std::string& message) {
    if (warned_[key]) return;
    warned_[key] = true;
    lastError_ = message;
    warnings_.push_back(message);
}

void DeviceBus::reset() {
    mouse_->reset();
    cursors_->reset();
    keyboard_->reset();
    controllers_->reset();
    midi_->reset();
    osc_->reset();
    vars_.clear();
    exitRequested = hideRequested = showRequested = false;
    warnings_.clear();
    warned_.clear();
    lastError_.clear();
    debugText_.clear();
    frames_ = 0;
    starting = false;
}

void DeviceBus::beginFrame() {
    keyboard_->dryRun = dryRun;
    mouse_->dryRun = dryRun;
    cursors_->dryRun = dryRun;
    controllers_->dryRun = dryRun;
    midi_->dryRun = dryRun;
    osc_->dryRun = dryRun;
    starting = (frames_ == 0);
    ++frames_;
    mouse_->poll();
    speech_->poll();
    controllers_->poll();
    osc_->poll();
}

void DeviceBus::endFrame() {
    keyboard_->endFrame();
    mouse_->endFrame();
}

void DeviceBus::releaseGeneratedOutput() {
    keyboard_->releaseAllHeld();
    mouse_->releaseAllButtons();
}

void DeviceBus::setVar(const std::string& name, const Value& value) { vars_[lower(name)] = value; }

Value DeviceBus::getVar(const std::string& name) const {
    auto it = vars_.find(lower(name));
    if (it == vars_.end()) return Value::null();
    return it->second;
}

Value DeviceBus::screenGet(const std::string& name) const {
    std::string key = lower(name);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (key == "desktopleft") return Value::number(vx, "px");
    if (key == "desktoptop") return Value::number(vy, "px");
    if (key == "desktopright") return Value::number(vx + vw - 1, "px");
    if (key == "desktopbottom") return Value::number(vy + vh - 1, "px");
    if (key == "desktopwidth") return Value::number(vw, "px");
    if (key == "desktopheight") return Value::number(vh, "px");
    if (key == "width") return Value::number(GetSystemMetrics(SM_CXSCREEN), "px");
    if (key == "height") return Value::number(GetSystemMetrics(SM_CYSCREEN), "px");
    if (key == "left" || key == "top") return Value::number(0, "px");
    if (key == "right") return Value::number(GetSystemMetrics(SM_CXSCREEN) - 1, "px");
    if (key == "bottom") return Value::number(GetSystemMetrics(SM_CYSCREEN) - 1, "px");
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (key == "workarealeft") return Value::number(wa.left, "px");
    if (key == "workareatop") return Value::number(wa.top, "px");
    if (key == "workarearight") return Value::number(wa.right - 1, "px");
    if (key == "workareabottom") return Value::number(wa.bottom - 1, "px");
    if (key == "workareawidth") return Value::number(wa.right - wa.left, "px");
    if (key == "workareaheight") return Value::number(wa.bottom - wa.top, "px");
    HDC dc = GetDC(nullptr);
    int mmw = dc ? GetDeviceCaps(dc, HORZSIZE) : 0;
    int mmh = dc ? GetDeviceCaps(dc, VERTSIZE) : 0;
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    if (key == "physicalwidth") return Value::number(mmw / 10.0, "cm");
    if (key == "physicalheight") return Value::number(mmh / 10.0, "cm");
    if (key == "viewablesize") return Value::number(std::hypot(mmw, mmh) / 10.0, "cm");
    if (key == "pixelsperinch") return Value::number(dpi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    if (key == "orientation") {
        if (dm.dmDisplayOrientation == DMDO_90) return Value::number(90, "deg");
        if (dm.dmDisplayOrientation == DMDO_180) return Value::number(180, "deg");
        if (dm.dmDisplayOrientation == DMDO_270) return Value::number(270, "deg");
        return Value::number(0, "deg");
    }
    if (key == "screensaverenabled") {
        BOOL on = FALSE;
        SystemParametersInfoW(SPI_GETSCREENSAVEACTIVE, 0, &on, 0);
        return Value::boolean(on != FALSE);
    }
    if (key == "screensaverrunning") {
        BOOL on = FALSE;
        SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &on, 0);
        return Value::boolean(on != FALSE);
    }
    if (key == "screensavertimeout") {
        DWORD sec = 0;
        SystemParametersInfoW(SPI_GETSCREENSAVETIMEOUT, 0, &sec, 0);
        return Value::number(sec, "s");
    }
    if (key == "scrollx" || key == "scrolly") return Value::number(0, "px");
    return Value::number(0, "px");
}

void DeviceBus::screenSet(const std::string& name, const Value& value) {
    std::string key = lower(name);
    if (key == "orientation") {
        int deg = static_cast<int>(value.asNumber());
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) return;
        dm.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;
        if (deg % 180 != 0) std::swap(dm.dmPelsWidth, dm.dmPelsHeight);
        if (deg % 360 == 90) dm.dmDisplayOrientation = DMDO_90;
        else if (deg % 360 == 180) dm.dmDisplayOrientation = DMDO_180;
        else if (deg % 360 == 270) dm.dmDisplayOrientation = DMDO_270;
        else dm.dmDisplayOrientation = DMDO_DEFAULT;
        ChangeDisplaySettingsW(&dm, 0);
    }
}

Value DeviceBus::windowGet(const std::string& name) const {
    std::string key = lower(name);
    HWND w = GetForegroundWindow();
    wchar_t titleW[512]{};
    wchar_t classW[256]{};
    if (w) {
        GetWindowTextW(w, titleW, 512);
        GetClassNameW(w, classW, 256);
    }
    auto toUtf8 = [](const wchar_t* ws) {
        int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        std::string s(n > 0 ? n - 1 : 0, '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), n, nullptr, nullptr);
        return s;
    };
    std::string title = toUtf8(titleW);
    std::string cls = toUtf8(classW);
    if (key == "title") return Value::string(title);
    if (key == "class") return Value::string(cls);
    size_t dash = title.find(" - ");
    size_t skip = 3;
    if (dash == std::string::npos) {
        dash = title.find('-');
        skip = 1;
    }
    if (key == "beforedash") return Value::string(dash == std::string::npos ? title : title.substr(0, dash));
    if (key == "afterdash")
        return Value::string(dash == std::string::npos ? std::string() : title.substr(dash + skip));
    if (key == "appname" || key == "filename") {
        DWORD pid = 0;
        if (w) GetWindowThreadProcessId(w, &pid);
        HANDLE proc = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : nullptr;
        wchar_t path[MAX_PATH]{};
        DWORD n = MAX_PATH;
        if (proc) QueryFullProcessImageNameW(proc, 0, path, &n);
        if (proc) CloseHandle(proc);
        std::string full = toUtf8(path);
        if (key == "filename") {
            auto sl = full.find_last_of("\\/");
            return Value::string(sl == std::string::npos ? full : full.substr(sl + 1));
        }
        return Value::string(full);
    }
    RECT r{}, c{};
    if (w) {
        GetWindowRect(w, &r);
        GetClientRect(w, &c);
    }
    if (key == "left") return Value::number(r.left, "px");
    if (key == "top") return Value::number(r.top, "px");
    if (key == "width") return Value::number(r.right - r.left, "px");
    if (key == "height") return Value::number(r.bottom - r.top, "px");
    if (key == "clientwidth") return Value::number(c.right - c.left, "px");
    if (key == "clientheight") return Value::number(c.bottom - c.top, "px");
    return Value::string("");
}

Value DeviceBus::touchpadGet(const std::string& name) {
    std::string key = lower(name);
    POINT p{};
    GetCursorPos(&p);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (key == "x") return Value::number(sw ? p.x / static_cast<double>(sw) : 0);
    if (key == "y") return Value::number(sh ? p.y / static_cast<double>(sh) : 0);
    if (key == "z" || key == "w") return Value::number(0);
    if (key == "fingerpresent" || key == "styluspresent")
        return Value::boolean((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    if (key == "leftbutton") return mouse_->get("leftbutton");
    if (key == "rightbutton") return mouse_->get("rightbutton");
    if (key == "middlebutton") return mouse_->get("middlebutton");
    if (key == "dpadup" || key == "dpaddown" || key == "dpadleft" || key == "dpadright") return Value::boolean(false);
    if (key == "swallow") return mouse_->get("swallow");
    return Value::boolean(false);
}

void DeviceBus::touchpadSet(const std::string& name, const Value& value) {
    if (lower(name) == "swallow") mouse_->set("swallow", value);
}

void DeviceBus::sendOsc(const std::string& ip, int port, const std::string& address, const std::vector<Value>& args) {
    osc_->send(ip, port, address, args);
}

Value DeviceBus::get(const std::string& pathRaw) {
    std::string path = lower(pathRaw);
    if (path == "debug") return Value::string(debugText_);
    if (path == "starting") return Value::boolean(starting);

    auto dot = path.find('.');
    std::string root = dot == std::string::npos ? path : path.substr(0, dot);
    std::string rest = dot == std::string::npos ? std::string() : path.substr(dot + 1);

    if (root == "var") return getVar(rest);
    int keyIndex = 0;
    if (indexedName(root, "keyboard", keyIndex) || indexedName(root, "key", keyIndex)) {
        if (rest == "exists" || rest == "present") return Value::boolean(keyboard_->get("count").asNumber() >= keyIndex);
        return keyboard_->get(rest);
    }
    if (root == "key" || root == "keyboard") return keyboard_->get(rest);
    int mouseIndex = 0;
    if (indexedName(root, "mouse", mouseIndex)) return mouse_->getIndexed(mouseIndex, rest);
    if (root == "mouse" || root == "realmouse") return mouse_->get(rest);
    if (root == "fakemouse") {
        if (rest == "directinputx") return Value::number(mouse_->fakeDirectX(), "mickeys");
        if (rest == "directinputy") return Value::number(mouse_->fakeDirectY(), "mickeys");
        if (rest == "directinput2d")
            return Value::vector({static_cast<double>(mouse_->fakeDirectX()), static_cast<double>(mouse_->fakeDirectY())},
                                 "mickeys");
        return mouse_->get(rest);
    }
    int cursorIndex = 0;
    if (indexedName(root, "cursor", cursorIndex)) return cursors_->get(cursorIndex, rest);
    if (root == "cursor") return mouse_->get(rest.empty() ? "cursorposx" : rest);
    int screenIndex = 0;
    if (root == "screen" || indexedName(root, "screen", screenIndex)) return screenGet(rest);
    if (root == "window") return windowGet(rest);
    if (root == "touchpad") return touchpadGet(rest);
    if (osc_->handles(root)) return osc_->get(root, rest);
    if (root == "speech" || root == "microphone") return speech_->getProperty(root, rest);
    if (midi_->handles(root)) return midi_->get(root, rest);
    if (controllers_->handles(root)) return controllers_->get(root, rest);
    if (root == "pie") {
        if (rest == "framerate") return Value::number(60);
        if (rest == "mousedpi") return Value::number(mouseDpi);
        if (rest == "epsilon") return Value::number(pieEpsilon);
        return Value::null();
    }
    if (isUnsupportedDevice(root)) {
        warnOnce(root, "Unsupported device '" + root + "'.");
        return Value::boolean(false);
    }
    if (rest.empty() && KeyboardDevice::isKnownKey(root)) return keyboard_->get(root);
    warnOnce(path, "Unknown identifier '" + pathRaw + "'.");
    return Value::boolean(false);
}

void DeviceBus::set(const std::string& pathRaw, const Value& value, bool force) {
    (void)force;
    std::string path = lower(pathRaw);
    if (path == "debug") {
        debugText_ = value.asString();
        if (debugHandler_) debugHandler_(debugText_);
        return;
    }
    auto dot = path.find('.');
    std::string root = dot == std::string::npos ? path : path.substr(0, dot);
    std::string rest = dot == std::string::npos ? std::string() : path.substr(dot + 1);

    if (root == "var") {
        setVar(rest, value);
        return;
    }
    int keyIndex = 0;
    if (root == "key" || root == "keyboard" || indexedName(root, "keyboard", keyIndex) ||
        indexedName(root, "key", keyIndex)) {
        keyboard_->setProperty(rest, value);
        return;
    }
    int mouseIndex = 0;
    if (indexedName(root, "mouse", mouseIndex)) {
        warnOnce(root, "Individual mice (mouse" + std::to_string(mouseIndex) + ") are read-only.");
        return;
    }
    if (root == "mouse" || root == "fakemouse" || root == "realmouse") {
        mouse_->set(rest, value);
        return;
    }
    int cursorIndex = 0;
    if (indexedName(root, "cursor", cursorIndex)) {
        cursors_->set(cursorIndex, rest, value);
        return;
    }
    if (root == "cursor") {
        mouse_->set(rest, value);
        return;
    }
    if (root == "pie") {
        if (rest == "mousedpi") mouseDpi = value.asNumber();
        if (rest == "epsilon") pieEpsilon = value.asNumber();
        return;
    }
    int screenIndex = 0;
    if (root == "screen" || indexedName(root, "screen", screenIndex)) {
        screenSet(rest, value);
        return;
    }
    if (root == "touchpad") {
        touchpadSet(rest, value);
        return;
    }
    if (osc_->handles(root)) {
        osc_->set(root, rest, value);
        return;
    }
    if (root == "speech" || root == "microphone") {
        speech_->setProperty(root, rest, value);
        return;
    }
    if (midi_->handles(root)) {
        midi_->set(root, rest, value);
        return;
    }
    if (controllers_->handles(root)) {
        controllers_->set(root, rest, value);
        return;
    }
    if (isUnsupportedDevice(root)) {
        warnOnce(root, "Unsupported device '" + root + "'.");
        return;
    }
    if (rest.empty()) {
        keyboard_->setHeld(root, value.isTruthy());
        return;
    }
    warnOnce(path, "Unknown assignment target '" + pathRaw + "'.");
}
