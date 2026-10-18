#include "MouseDevice.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <vector>

#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x01000
#endif

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC 0x01
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE 0x02
#endif

namespace {
const wchar_t* kRawClass = L"DarkPIERawMouse";
MouseDevice* g_hookOwner = nullptr;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

long long nowMs() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
}

LRESULT CALLBACK rawWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INPUT) {
        auto* mouse = reinterpret_cast<MouseDevice*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        if (!mouse || size == 0) return 0;
        std::string buf(size, '\0');
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) ==
            (UINT)-1)
            return 0;
        auto* raw = reinterpret_cast<RAWINPUT*>(buf.data());
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            const RAWMOUSE& m = raw->data.mouse;
            int dx = 0, dy = 0;
            if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                dx = m.lLastX;
                dy = m.lLastY;
            }
            mouse->handleRaw(raw->header.hDevice, dx, dy, m.usButtonFlags);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK swallowHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && g_hookOwner) {
        auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (!(info->flags & LLMHF_INJECTED)) return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
}

MouseDevice::MouseDevice() { ensureRawInput(); }

MouseDevice::~MouseDevice() {
    for (const auto& b : physicallyHeld_) sendButton(b, false);
    physicallyHeld_.clear();
    desiredHeld_.clear();
    setSwallow(false);
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
}

void MouseDevice::addRawDelta(int dx, int dy) {
    pendingRawX_ += dx;
    pendingRawY_ += dy;
}

std::string MouseDevice::buttonFromFlags(unsigned short flags, bool& down, bool& known) {
    known = true;
    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
        down = true;
        return "leftbutton";
    }
    if (flags & RI_MOUSE_LEFT_BUTTON_UP) {
        down = false;
        return "leftbutton";
    }
    if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
        down = true;
        return "rightbutton";
    }
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP) {
        down = false;
        return "rightbutton";
    }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
        down = true;
        return "middlebutton";
    }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) {
        down = false;
        return "middlebutton";
    }
    if (flags & RI_MOUSE_BUTTON_4_DOWN) {
        down = true;
        return "xbutton1";
    }
    if (flags & RI_MOUSE_BUTTON_4_UP) {
        down = false;
        return "xbutton1";
    }
    if (flags & RI_MOUSE_BUTTON_5_DOWN) {
        down = true;
        return "xbutton2";
    }
    if (flags & RI_MOUSE_BUTTON_5_UP) {
        down = false;
        return "xbutton2";
    }
    known = false;
    return {};
}

MouseDevice::Phys& MouseDevice::phys(int index) {
    if (index < 1) index = 1;
    if (static_cast<int>(mice_.size()) < index) mice_.resize(static_cast<size_t>(index));
    return mice_[static_cast<size_t>(index - 1)];
}

void MouseDevice::handleRaw(void* headerDevice, int dx, int dy, unsigned short buttonFlags) {
    addRawDelta(dx, dy);
    auto it = handleIndex_.find(headerDevice);
    int idx = 0;
    if (it == handleIndex_.end()) {
        idx = static_cast<int>(mice_.size()) + 1;
        handleIndex_[headerDevice] = idx;
        phys(idx).handle = headerDevice;
        phys(idx).present = true;
    } else {
        idx = it->second;
    }
    Phys& m = phys(idx);
    m.x += dx;
    m.y += dy;
    bool down = false, known = false;
    std::string btn = buttonFromFlags(buttonFlags, down, known);
    if (known) {
        if (down) m.buttons.insert(btn);
        else m.buttons.erase(btn);
    }
}

void MouseDevice::refreshPhysicalMice() {
    UINT n = 0;
    GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST));
    if (n == 0) return;
    std::vector<RAWINPUTDEVICELIST> list(n);
    if (GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) return;
    std::vector<HANDLE> found;
    for (UINT i = 0; i < n; ++i)
        if (list[i].dwType == RIM_TYPEMOUSE) found.push_back(list[i].hDevice);
    std::sort(found.begin(), found.end());
    for (auto& m : mice_) m.present = false;
    int next = 1;
    for (HANDLE h : found) {
        if (!handleIndex_.count(h)) {
            while (next <= static_cast<int>(mice_.size()) && phys(next).handle) ++next;
            handleIndex_[h] = next++;
        }
        int idx = handleIndex_[h];
        Phys& m = phys(idx);
        m.handle = h;
        m.present = true;
    }
}

void MouseDevice::setSwallow(bool on) {
    swallow_ = on;
    if (dryRun) return;
    if (on && !swallowHook_) {
        g_hookOwner = this;
        swallowHook_ = SetWindowsHookExW(WH_MOUSE_LL, swallowHookProc, GetModuleHandleW(nullptr), 0);
    } else if (!on && swallowHook_) {
        UnhookWindowsHookEx(static_cast<HHOOK>(swallowHook_));
        swallowHook_ = nullptr;
        if (g_hookOwner == this) g_hookOwner = nullptr;
    }
}

void MouseDevice::reset() {
    desiredHeld_.clear();
    for (const auto& b : physicallyHeld_) sendButton(b, false);
    physicallyHeld_.clear();
    setSwallow(false);
    fakeDirectX_ = fakeDirectY_ = 0;
    combinedRealX_ = combinedRealY_ = 0;
    pendingRawX_ = pendingRawY_ = 0;
    stickyX_ = stickyY_ = 0;
    handleIndex_.clear();
    mice_.clear();
}

void MouseDevice::ensureRawInput() {
    if (rawReady_) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = rawWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kRawClass;
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, kRawClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    hwnd_ = hwnd;
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    rawReady_ = RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE;
}

void MouseDevice::pumpRawInput() {
    if (!hwnd_) return;
    MSG msg{};
    while (PeekMessageW(&msg, static_cast<HWND>(hwnd_), 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

std::string MouseDevice::canonicalize(std::string name) {
    name = lower(std::move(name));
    name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
    if (name == "left" || name == "lbutton") name = "leftbutton";
    if (name == "right" || name == "rbutton") name = "rightbutton";
    if (name == "middle" || name == "mbutton") name = "middlebutton";
    if (name == "x1" || name == "button4") name = "xbutton1";
    if (name == "x2" || name == "button5") name = "xbutton2";
    if (name == "wheel" || name == "scrollup") name = "wheelup";
    if (name == "scrolldown") name = "wheeldown";
    if (name == "scrollleft") name = "wheelleft";
    if (name == "scrollright") name = "wheelright";
    if (name == "posx") name = "cursorposx";
    if (name == "posy") name = "cursorposy";
    return name;
}

bool MouseDevice::isButton(const std::string& name) const {
    return name == "leftbutton" || name == "rightbutton" || name == "middlebutton" ||
           name == "xbutton1" || name == "xbutton2" || name == "button6" || name == "button7" ||
           name == "button8";
}

void MouseDevice::sendButton(const std::string& name, bool down) {
    if (dryRun) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    if (name == "leftbutton") in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else if (name == "rightbutton") in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    else if (name == "middlebutton") in.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    else if (name == "xbutton1" || name == "xbutton2") {
        in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        in.mi.mouseData = (name == "xbutton1") ? XBUTTON1 : XBUTTON2;
    } else {
        return;
    }
    SendInput(1, &in, sizeof(INPUT));
}

void MouseDevice::sendWheel(int clicks, bool horizontal) {
    if (dryRun || clicks == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(clicks * WHEEL_DELTA);
    SendInput(1, &in, sizeof(INPUT));
}

void MouseDevice::setCursorPixel(int x, int y) {
    if (dryRun) return;
    SetCursorPos(x, y);
}

void MouseDevice::moveRelative(int dx, int dy) {
    if (dryRun || (dx == 0 && dy == 0)) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    SendInput(1, &in, sizeof(INPUT));
}

Value MouseDevice::getIndexed(int index, const std::string& name) {
    refreshPhysicalMice();
    std::string key = canonicalize(name);
    const Phys& m = phys(index);
    if (key == "present") return Value::boolean(m.present);
    if (isButton(key)) return Value::boolean(m.buttons.count(key) > 0);
    if (key == "directinputx") return Value::number(m.x, "mickeys");
    if (key == "directinputy") return Value::number(m.y, "mickeys");
    if (key == "directinput2d")
        return Value::vector({static_cast<double>(m.x), static_cast<double>(m.y)}, "mickeys");
    POINT p{};
    GetCursorPos(&p);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (key == "x") return Value::number(index <= 1 && sw ? p.x / static_cast<double>(sw) : 0);
    if (key == "y") return Value::number(index <= 1 && sh ? p.y / static_cast<double>(sh) : 0);
    return get(name);
}

Value MouseDevice::get(const std::string& name) {
    std::string key = canonicalize(name);
    if (key == "present") return Value::boolean(GetSystemMetrics(SM_MOUSEPRESENT) != 0);
    if (key == "swallow" || key == "swallowbuttons" || key == "swallowmovement" || key == "swallowwheels")
        return Value::boolean(swallow_);
    if (isButton(key) || key == "isdragging")
        return Value::boolean(desiredHeld_.count(key) > 0);
    if (key == "wheelpresent") return Value::boolean(true);

    POINT p{};
    GetCursorPos(&p);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (key == "count") {
        UINT n = 0;
        GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST));
        std::vector<RAWINPUTDEVICELIST> list(n);
        int count = 0;
        if (n && GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) != (UINT)-1) {
            for (UINT i = 0; i < n; ++i)
                if (list[i].dwType == RIM_TYPEMOUSE) ++count;
        }
        return Value::number(std::max(1, count));
    }
    if (key == "cursorvisible") {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        if (GetCursorInfo(&ci)) return Value::boolean((ci.flags & CURSOR_SHOWING) != 0);
        return Value::boolean(true);
    }
    if (key == "cursorposx") return Value::number(p.x, "px");
    if (key == "cursorposy") return Value::number(p.y, "px");
    if (key == "cursorpos") return Value::vector({static_cast<double>(p.x), static_cast<double>(p.y)}, "px");
    if (key == "cursor" || key == "permanentcursor") return Value::number(0);
    if (key == "directinputx") return Value::number(combinedRealX_ + fakeDirectX_, "mickeys");
    if (key == "directinputy") return Value::number(combinedRealY_ + fakeDirectY_, "mickeys");
    if (key == "directinput2d")
        return Value::vector({static_cast<double>(combinedRealX_ + fakeDirectX_),
                              static_cast<double>(combinedRealY_ + fakeDirectY_)},
                             "mickeys");
    if (key == "directinput3d")
        return Value::vector({static_cast<double>(combinedRealX_ + fakeDirectX_),
                              static_cast<double>(combinedRealY_ + fakeDirectY_), 0},
                             "mickeys");
    if (key == "directinputz" || key == "directinputh") return Value::number(0);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (key == "x") return Value::number(sw ? p.x / static_cast<double>(sw) : 0);
    if (key == "y") return Value::number(sh ? p.y / static_cast<double>(sh) : 0);
    if (key == "virtualdesktopx") return Value::number(vw ? (p.x - vx) / static_cast<double>(vw) : 0);
    if (key == "virtualdesktopy") return Value::number(vh ? (p.y - vy) / static_cast<double>(vh) : 0);
    if (key == "screenfraction")
        return Value::vector({sw ? p.x / static_cast<double>(sw) : 0, sh ? p.y / static_cast<double>(sh) : 0});
    if (key == "desktopfraction")
        return Value::vector({vw ? (p.x - vx) / static_cast<double>(vw) : 0,
                              vh ? (p.y - vy) / static_cast<double>(vh) : 0});
    if (key == "wheelscrolllines") return Value::number(3);
    if (key == "doubleclicktime") return Value::number(GetDoubleClickTime() / 1000.0);
    return Value::boolean(false);
}

void MouseDevice::poll() {
    ensureRawInput();
    pumpRawInput();
    refreshPhysicalMice();
    int dx = pendingRawX_;
    int dy = pendingRawY_;
    pendingRawX_ = 0;
    pendingRawY_ = 0;
    const long long t = nowMs();
    if (dx != 0 || dy != 0) {
        stickyX_ = dx;
        stickyY_ = dy;
        lastMoveMs_ = t;
    } else if (t - lastMoveMs_ < 80) {
        dx = stickyX_;
        dy = stickyY_;
    } else {
        stickyX_ = 0;
        stickyY_ = 0;
    }
    if (!dryRun) {
        combinedRealX_ += dx;
        combinedRealY_ += dy;
    }
}

void MouseDevice::set(const std::string& name, const Value& value) {
    std::string key = canonicalize(name);
    POINT p{};
    GetCursorPos(&p);

    if (key == "swallow" || key == "swallowbuttons" || key == "swallowmovement" || key == "swallowwheels") {
        setSwallow(value.isTruthy());
        return;
    }
    if (key == "wheelup" || key == "wheeldown") {
        if (value.isTruthy()) sendWheel(key == "wheelup" ? 1 : -1, false);
        return;
    }
    if (key == "wheelleft" || key == "wheelright") {
        if (value.isTruthy()) sendWheel(key == "wheelright" ? 1 : -1, true);
        return;
    }
    if (isButton(key)) {
        if (value.isTruthy()) desiredHeld_.insert(key);
        else desiredHeld_.erase(key);
        return;
    }
    if (key == "cursorposx") {
        setCursorPixel(static_cast<int>(value.asNumber()), p.y);
        return;
    }
    if (key == "cursorposy") {
        setCursorPixel(p.x, static_cast<int>(value.asNumber()));
        return;
    }
    if (key == "cursorpos") {
        auto v = value.asVector();
        int nx = v.size() > 0 ? static_cast<int>(v[0]) : p.x;
        int ny = v.size() > 1 ? static_cast<int>(v[1]) : p.y;
        setCursorPixel(nx, ny);
        return;
    }
    if (key == "cursor" || key == "permanentcursor" || key == "doubleclicktime") return;
    if (key == "x") {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        setCursorPixel(static_cast<int>(value.asNumber() * sw), p.y);
        return;
    }
    if (key == "y") {
        int sh = GetSystemMetrics(SM_CYSCREEN);
        setCursorPixel(p.x, static_cast<int>(value.asNumber() * sh));
        return;
    }
    if (key == "virtualdesktopx") {
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        setCursorPixel(vx + static_cast<int>(value.asNumber() * vw), p.y);
        return;
    }
    if (key == "virtualdesktopy") {
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        setCursorPixel(p.x, vy + static_cast<int>(value.asNumber() * vh));
        return;
    }
    if (key == "screenfraction" || key == "desktopfraction") {
        auto v = value.asVector();
        if (v.size() >= 1) set(key == "screenfraction" ? "x" : "virtualdesktopx", Value::number(v[0]));
        if (v.size() >= 2) set(key == "screenfraction" ? "y" : "virtualdesktopy", Value::number(v[1]));
        return;
    }
    if (key == "directinputx") {
        int combined = combinedRealX_ + fakeDirectX_;
        int nx = static_cast<int>(value.asNumber());
        moveRelative(nx - combined, 0);
        fakeDirectX_ += nx - combined;
        return;
    }
    if (key == "directinputy") {
        int combined = combinedRealY_ + fakeDirectY_;
        int ny = static_cast<int>(value.asNumber());
        moveRelative(0, ny - combined);
        fakeDirectY_ += ny - combined;
        return;
    }
    if (key == "directinput2d" || key == "directinput3d") {
        auto v = value.asVector();
        int cx = combinedRealX_ + fakeDirectX_;
        int cy = combinedRealY_ + fakeDirectY_;
        int nx = v.size() > 0 ? static_cast<int>(v[0]) : cx;
        int ny = v.size() > 1 ? static_cast<int>(v[1]) : cy;
        moveRelative(nx - cx, ny - cy);
        fakeDirectX_ += nx - cx;
        fakeDirectY_ += ny - cy;
        return;
    }
}

void MouseDevice::releaseAllButtons() { desiredHeld_.clear(); }

void MouseDevice::endFrame() {
    static const char* buttons[] = {"leftbutton", "rightbutton", "middlebutton", "xbutton1", "xbutton2"};
    for (auto b : buttons) {
        bool want = desiredHeld_.count(b) > 0;
        bool have = physicallyHeld_.count(b) > 0;
        if (want && !have) {
            sendButton(b, true);
            physicallyHeld_.insert(b);
        } else if (!want && have) {
            sendButton(b, false);
            physicallyHeld_.erase(b);
        }
    }
}
