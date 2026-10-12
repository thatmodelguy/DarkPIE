#include "CursorDevice.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace {
const wchar_t* kClass = L"DarkPIEFakeCursor";
bool classRegistered = false;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

COLORREF colorFor(int index) {
    static const COLORREF colors[] = {RGB(255, 64, 64), RGB(64, 200, 64), RGB(64, 140, 255), RGB(255, 200, 40),
                                      RGB(220, 80, 220), RGB(80, 220, 220)};
    if (index < 0) index = 0;
    return colors[static_cast<size_t>(index) % (sizeof(colors) / sizeof(colors[0]))];
}

void paintCursor(HDC hdc, int index, COLORREF color) {
    POINT arrow[] = {{0, 0}, {0, 16}, {4, 12}, {8, 20}, {10, 19}, {6, 11}, {12, 11}};
    HBRUSH fill = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ oldB = SelectObject(hdc, fill);
    HGDIOBJ oldP = SelectObject(hdc, pen);
    Polygon(hdc, arrow, 7);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    wchar_t label[16];
    wsprintfW(label, L"%d", index);
    TextOutW(hdc, 10, 12, label, lstrlenW(label));
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(fill);
    DeleteObject(pen);
}

LRESULT CALLBACK cursorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH clear = CreateSolidBrush(RGB(1, 1, 1));
        FillRect(hdc, &rc, clear);
        DeleteObject(clear);
        int index = static_cast<int>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        paintCursor(hdc, index, colorFor(index));
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ensureClass() {
    if (classRegistered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = cursorWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    classRegistered = true;
}
}

CursorDevice::CursorDevice() = default;

CursorDevice::~CursorDevice() { reset(); }

CursorDevice::Slot& CursorDevice::slot(int index) {
    auto& s = slots_[index];
    s.index = index;
    return s;
}

void CursorDevice::ensureWindow(Slot& s) {
    if (dryRun || s.hwnd) return;
    ensureClass();
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                kClass, L"", WS_POPUP, s.x, s.y, 36, 36, nullptr, nullptr, GetModuleHandleW(nullptr),
                                nullptr);
    if (!hwnd) return;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, s.index);
    SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 0, LWA_COLORKEY);
    s.hwnd = hwnd;
    s.created = true;
}

void CursorDevice::updateWindow(Slot& s) {
    if (dryRun) return;
    if (!s.visible) {
        if (s.hwnd) ShowWindow(static_cast<HWND>(s.hwnd), SW_HIDE);
        return;
    }
    ensureWindow(s);
    if (!s.hwnd) return;
    SetWindowPos(static_cast<HWND>(s.hwnd), HWND_TOPMOST, s.x, s.y, 36, 36, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(static_cast<HWND>(s.hwnd), nullptr, TRUE);
}

void CursorDevice::destroyWindow(Slot& s) {
    if (s.hwnd) {
        DestroyWindow(static_cast<HWND>(s.hwnd));
        s.hwnd = nullptr;
    }
}

void CursorDevice::sendClick(int x, int y, const char* button, bool down) {
    if (dryRun) return;
    POINT p{x, y};
    HWND hwnd = WindowFromPoint(p);
    if (!hwnd) return;
    ScreenToClient(hwnd, &p);
    LPARAM lp = MAKELPARAM(static_cast<WORD>(p.x), static_cast<WORD>(p.y));
    UINT msg = WM_MOUSEMOVE;
    WPARAM wp = 0;
    if (std::strcmp(button, "left") == 0) {
        msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
        wp = MK_LBUTTON;
    } else if (std::strcmp(button, "right") == 0) {
        msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
        wp = MK_RBUTTON;
    } else if (std::strcmp(button, "middle") == 0) {
        msg = down ? WM_MBUTTONDOWN : WM_MBUTTONUP;
        wp = MK_MBUTTON;
    }
    PostMessageW(hwnd, WM_MOUSEMOVE, wp, lp);
    PostMessageW(hwnd, msg, wp, lp);
}

void CursorDevice::reset() {
    for (auto& kv : slots_) destroyWindow(kv.second);
    slots_.clear();
}

Value CursorDevice::get(int index, const std::string& name) {
    std::string key = lower(name);
    Slot& s = slot(index);
    if (key == "visible") return Value::boolean(s.visible);
    if (key == "posx" || key == "cursorposx" || key == "x") return Value::number(s.x, "px");
    if (key == "posy" || key == "cursorposy" || key == "y") return Value::number(s.y, "px");
    if (key == "leftbutton") return Value::boolean(s.left);
    if (key == "rightbutton") return Value::boolean(s.right);
    if (key == "middlebutton") return Value::boolean(s.middle);
    return Value::boolean(false);
}

void CursorDevice::set(int index, const std::string& name, const Value& value) {
    std::string key = lower(name);
    Slot& s = slot(index);
    if (key == "visible") {
        s.visible = value.isTruthy();
        updateWindow(s);
        return;
    }
    if (key == "posx" || key == "cursorposx" || key == "x") {
        s.x = static_cast<int>(std::lround(value.asNumber()));
        updateWindow(s);
        return;
    }
    if (key == "posy" || key == "cursorposy" || key == "y") {
        s.y = static_cast<int>(std::lround(value.asNumber()));
        updateWindow(s);
        return;
    }
    auto applyBtn = [&](bool Slot::* mem, const char* which) {
        bool next = value.isTruthy();
        if ((s.*mem) != next) {
            s.*mem = next;
            if (s.visible) sendClick(s.x, s.y, which, next);
        }
    };
    if (key == "leftbutton") applyBtn(&Slot::left, "left");
    else if (key == "rightbutton") applyBtn(&Slot::right, "right");
    else if (key == "middlebutton") applyBtn(&Slot::middle, "middle");
}
