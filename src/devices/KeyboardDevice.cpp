#include "KeyboardDevice.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <vector>

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

KeyboardDevice* g_kbHookOwner = nullptr;

bool parseUnicodeName(const std::string& key, wchar_t& ch) {
    if (key.size() < 8 || key.rfind("unicode", 0) != 0) return false;
    unsigned v = 0;
    for (size_t i = 7; i < key.size(); ++i) {
        char c = key[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
        else return false;
    }
    ch = static_cast<wchar_t>(v);
    return true;
}

bool parseScanName(const std::string& key, WORD& scan) {
    if (key.size() < 5 || key.rfind("scan", 0) != 0) return false;
    unsigned v = 0;
    for (size_t i = 4; i < key.size(); ++i) {
        char c = key[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
        else return false;
    }
    scan = static_cast<WORD>(v);
    return true;
}

std::string trimCopy(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}

std::vector<std::string> splitKeyCombo(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    if (!s.empty() && s[0] == '^') s = "ctrl+" + s.substr(1);
    std::string flat;
    for (size_t i = 0; i < s.size();) {
        if (i + 3 <= s.size() && s.compare(i, 3, "and") == 0) {
            const bool leftOk = i == 0 || !std::isalnum(static_cast<unsigned char>(s[i - 1]));
            const bool rightOk = i + 3 == s.size() || !std::isalnum(static_cast<unsigned char>(s[i + 3]));
            if (leftOk && rightOk) {
                flat.push_back('+');
                i += 3;
                continue;
            }
        }
        flat.push_back(s[i]);
        ++i;
    }
    std::vector<std::string> parts;
    std::string cur;
    for (char c : flat) {
        if (c == '+') {
            cur = trimCopy(cur);
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    cur = trimCopy(cur);
    if (!cur.empty()) parts.push_back(cur);
    for (auto& p : parts) {
        if (p.rfind("key.", 0) == 0) p = p.substr(4);
        else if (p.rfind("keyboard.", 0) == 0) p = p.substr(9);
    }
    return parts;
}

int appCommandFor(const std::string& key);

bool isSingleKeyName(const std::string& key, const std::unordered_map<std::string, unsigned short>& map) {
    wchar_t ch = 0;
    WORD scan = 0;
    if (parseUnicodeName(key, ch) || parseScanName(key, scan) || appCommandFor(key)) return true;
    auto it = map.find(key);
    return it != map.end();
}

int appCommandFor(const std::string& key) {
    if (key == "undo") return APPCOMMAND_UNDO;
    if (key == "redo") return APPCOMMAND_REDO;
    if (key == "new") return APPCOMMAND_NEW;
    if (key == "open") return APPCOMMAND_OPEN;
    if (key == "close") return APPCOMMAND_CLOSE;
    if (key == "save") return APPCOMMAND_SAVE;
    if (key == "reply") return APPCOMMAND_REPLY_TO_MAIL;
    if (key == "forward") return APPCOMMAND_FORWARD_MAIL;
    if (key == "send") return APPCOMMAND_SEND_MAIL;
    if (key == "spell") return APPCOMMAND_SPELL_CHECK;
    return 0;
}

LRESULT CALLBACK keyboardSwallowProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && g_kbHookOwner) {
        auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (!(info->flags & LLKHF_INJECTED) && g_kbHookOwner->isSwallowedVk(info->vkCode)) return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
}

KeyboardDevice::~KeyboardDevice() { reset(); }

KeyboardDevice::KeyboardDevice() {
    auto add = [&](const char* name, unsigned short vk) { map_[name] = vk; };
    for (char c = 'a'; c <= 'z'; ++c) {
        add(std::string(1, c).c_str(), static_cast<unsigned short>('A' + (c - 'a')));
    }
    for (char c = '0'; c <= '9'; ++c) {
        add(std::string(1, c).c_str(), static_cast<unsigned short>(c));
    }
    add("enter", VK_RETURN);
    add("return", VK_RETURN);
    add("escape", VK_ESCAPE);
    add("esc", VK_ESCAPE);
    add("space", VK_SPACE);
    add("spacebar", VK_SPACE);
    add("tab", VK_TAB);
    add("backspace", VK_BACK);
    add("delete", VK_DELETE);
    add("del", VK_DELETE);
    add("insert", VK_INSERT);
    add("ins", VK_INSERT);
    add("home", VK_HOME);
    add("end", VK_END);
    add("pageup", VK_PRIOR);
    add("pgup", VK_PRIOR);
    add("pagedown", VK_NEXT);
    add("pgdn", VK_NEXT);
    add("up", VK_UP);
    add("down", VK_DOWN);
    add("left", VK_LEFT);
    add("right", VK_RIGHT);
    add("shift", VK_SHIFT);
    add("lshift", VK_LSHIFT);
    add("leftshift", VK_LSHIFT);
    add("rshift", VK_RSHIFT);
    add("rightshift", VK_RSHIFT);
    add("ctrl", VK_CONTROL);
    add("control", VK_CONTROL);
    add("lctrl", VK_LCONTROL);
    add("leftctrl", VK_LCONTROL);
    add("leftcontrol", VK_LCONTROL);
    add("lcontrol", VK_LCONTROL);
    add("rctrl", VK_RCONTROL);
    add("rightctrl", VK_RCONTROL);
    add("rightcontrol", VK_RCONTROL);
    add("alt", VK_MENU);
    add("lalt", VK_LMENU);
    add("leftalt", VK_LMENU);
    add("ralt", VK_RMENU);
    add("rightalt", VK_RMENU);
    add("windows", VK_LWIN);
    add("win", VK_LWIN);
    add("leftwindows", VK_LWIN);
    add("lwin", VK_LWIN);
    add("rightwindows", VK_RWIN);
    add("rwin", VK_RWIN);
    add("apps", VK_APPS);
    add("capslock", VK_CAPITAL);
    add("numlock", VK_NUMLOCK);
    add("scrolllock", VK_SCROLL);
    add("printscreen", VK_SNAPSHOT);
    add("pause", VK_PAUSE);
    for (int i = 1; i <= 24; ++i) {
        add(("f" + std::to_string(i)).c_str(), static_cast<unsigned short>(VK_F1 + i - 1));
    }
    for (int i = 0; i <= 9; ++i) {
        add(("numpad" + std::to_string(i)).c_str(), static_cast<unsigned short>(VK_NUMPAD0 + i));
        add(("np" + std::to_string(i)).c_str(), static_cast<unsigned short>(VK_NUMPAD0 + i));
    }
    add("numpadenter", VK_RETURN);
    add("numpadplus", VK_ADD);
    add("numpadminus", VK_SUBTRACT);
    add("numpadmultiply", VK_MULTIPLY);
    add("numpaddivide", VK_DIVIDE);
    add("numpaddot", VK_DECIMAL);
    add("four", '4');
    add("one", '1');
    add("two", '2');
    add("three", '3');
    add("five", '5');
    add("six", '6');
    add("seven", '7');
    add("eight", '8');
    add("nine", '9');
    add("zero", '0');
    for (int i = 0; i <= 9; ++i) {
        std::string d(1, static_cast<char>('0' + i));
        add(("d" + d).c_str(), static_cast<unsigned short>('0' + i));
        add(("digit" + d).c_str(), static_cast<unsigned short>('0' + i));
        add(("number" + d).c_str(), static_cast<unsigned short>('0' + i));
        add(("num" + d).c_str(), static_cast<unsigned short>('0' + i));
    }
    add("minus", VK_OEM_MINUS);
    add("dash", VK_OEM_MINUS);
    add("hyphen", VK_OEM_MINUS);
    add("underscore", VK_OEM_MINUS);
    add("plus", VK_OEM_PLUS);
    add("equals", VK_OEM_PLUS);
    add("equal", VK_OEM_PLUS);
    add("comma", VK_OEM_COMMA);
    add("period", VK_OEM_PERIOD);
    add("dot", VK_OEM_PERIOD);
    add("slash", VK_OEM_2);
    add("question", VK_OEM_2);
    add("backslash", VK_OEM_5);
    add("pipe", VK_OEM_5);
    add("semicolon", VK_OEM_1);
    add("colon", VK_OEM_1);
    add("quote", VK_OEM_7);
    add("apostrophe", VK_OEM_7);
    add("tilde", VK_OEM_3);
    add("grave", VK_OEM_3);
    add("backtick", VK_OEM_3);
    add("lbracket", VK_OEM_4);
    add("leftbracket", VK_OEM_4);
    add("opensquare", VK_OEM_4);
    add("rbracket", VK_OEM_6);
    add("rightbracket", VK_OEM_6);
    add("closesquare", VK_OEM_6);
    add("menu", VK_APPS);
    add("contextmenu", VK_APPS);
    add("print", VK_PRINT);
    add("snapshot", VK_SNAPSHOT);
    add("sysrq", VK_SNAPSHOT);
    add("break", VK_PAUSE);
    add("console", VK_OEM_3);
    add("divide", VK_DIVIDE);
    add("multiply", VK_MULTIPLY);
    add("appmenu", VK_APPS);
    add("help", VK_HELP);
    add("mute", VK_VOLUME_MUTE);
    add("volumeup", VK_VOLUME_UP);
    add("volumedown", VK_VOLUME_DOWN);
    add("playpause", VK_MEDIA_PLAY_PAUSE);
    add("mediastop", VK_MEDIA_STOP);
    add("mediapause", VK_MEDIA_PLAY_PAUSE);
    add("prevtrack", VK_MEDIA_PREV_TRACK);
    add("nexttrack", VK_MEDIA_NEXT_TRACK);
    add("mail", VK_LAUNCH_MAIL);
    add("calculator", VK_LAUNCH_APP2);
    add("webhome", VK_BROWSER_HOME);
    add("sysreq", VK_SNAPSHOT);
    add("numpadsubtract", VK_SUBTRACT);
    add("npminus", VK_SUBTRACT);
    add("npplus", VK_ADD);
    add("npdot", VK_DECIMAL);
    add("npenter", VK_RETURN);
    add("npdivide", VK_DIVIDE);
    add("npmultiply", VK_MULTIPLY);
    add("mediaselect", VK_LAUNCH_MEDIA_SELECT);
    add("sleep", VK_SLEEP);
    add("mypictures", VK_LAUNCH_APP1);
    add("mymusic", VK_LAUNCH_MEDIA_SELECT);
    add("officehome", VK_LAUNCH_MEDIA_SELECT);
    add("browserback", VK_BROWSER_BACK);
    add("browserforward", VK_BROWSER_FORWARD);
    add("browserrefresh", VK_BROWSER_REFRESH);
    add("browserstop", VK_BROWSER_STOP);
    add("browsersearch", VK_BROWSER_SEARCH);
    add("browserfavorites", VK_BROWSER_FAVORITES);
    add("undo", 0);
    add("redo", 0);
    add("new", 0);
    add("open", 0);
    add("close", 0);
    add("reply", 0);
    add("forward", 0);
    add("send", 0);
    add("spell", 0);
    add("save", 0);
    add("messenger", 0);
    add("logoff", 0);
    add("euro", 0);
    add("dollar", 0);
}

std::string KeyboardDevice::canonicalize(std::string name) {
    name = lower(std::move(name));
    name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
    return name;
}

bool KeyboardDevice::isKnownKey(const std::string& name) {
    auto parts = splitKeyCombo(name);
    if (parts.empty()) return false;
    KeyboardDevice dummy;
    dummy.dryRun = true;
    for (const auto& p : parts) {
        if (!isSingleKeyName(p, dummy.map_)) return false;
    }
    return true;
}

unsigned short KeyboardDevice::vkFor(const std::string& name) const {
    auto it = map_.find(canonicalize(name));
    if (it == map_.end()) return 0;
    return it->second;
}

void KeyboardDevice::sendNamed(const std::string& key, bool down) {
    if (dryRun) return;
    wchar_t ch = 0;
    if (parseUnicodeName(key, ch)) {
        sendUnicode(ch, down);
        return;
    }
    WORD scan = 0;
    if (parseScanName(key, scan)) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = scan;
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
        return;
    }
    int app = appCommandFor(key);
    if (app) {
        if (down) {
            HWND w = GetForegroundWindow();
            if (w) SendMessageW(w, WM_APPCOMMAND, reinterpret_cast<WPARAM>(w), MAKELPARAM(0, app));
        }
        return;
    }
    unsigned short vk = vkFor(key);
    if (vk) sendVk(vk, down);
}

Value KeyboardDevice::get(const std::string& keyName) {
    auto parts = splitKeyCombo(keyName);
    if (parts.size() > 1) {
        for (const auto& p : parts) {
            if (!get(p).isTruthy()) return Value::boolean(false);
        }
        return Value::boolean(true);
    }
    std::string key = parts.empty() ? canonicalize(keyName) : parts[0];
    if (key == "anykey") return Value::boolean(anyKeyDown());
    if (key == "count") return Value::number(keyboardCount());
    if (key == "exists") return Value::boolean(keyboardCount() > 0);
    if (key == "repeatfakekeys") return Value::boolean(repeatFakeKeys_);
    if (key == "repeatmultiplefakekeys") return Value::boolean(repeatMultipleFakeKeys_);
    if (key == "repeatrate") return Value::number(repeatRateHz_, "hz");
    if (key == "repeatdelay") return Value::number(repeatDelaySec_, "s");
    if (key == "rawinput") return Value::boolean(true);
    if (key == "scancode" || key == "vk" || key == "row") return Value::number(key == "row" ? -1 : 0);
    if (desiredHeld_.count(key)) return Value::boolean(true);
    wchar_t ch = 0;
    WORD scan = 0;
    if (parseUnicodeName(key, ch) || parseScanName(key, scan) || appCommandFor(key))
        return Value::boolean(false);
    unsigned short vk = vkFor(key);
    if (!vk) return Value::boolean(false);
    if (dryRun) return Value::boolean(false);
    return Value::boolean((GetAsyncKeyState(vk) & 0x8000) != 0);
}

void KeyboardDevice::sendVk(unsigned short vk, bool down) {
    if (dryRun || !vk) return;
    const bool extended = vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
                          vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT || vk == VK_RIGHT ||
                          vk == VK_UP || vk == VK_DOWN || vk == VK_NUMLOCK || vk == VK_DIVIDE ||
                          vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN ||
                          vk == VK_APPS;
    WORD scan = static_cast<WORD>(MapVirtualKey(vk, MAPVK_VK_TO_VSC_EX));
    if (scan == 0) scan = static_cast<WORD>(MapVirtualKey(vk, MAPVK_VK_TO_VSC));

    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (extended ? KEYEVENTF_EXTENDEDKEY : 0) |
                    (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof(INPUT));
}

void KeyboardDevice::sendUnicode(wchar_t ch, bool down) {
    if (dryRun) return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = static_cast<WORD>(ch);
    in.ki.dwFlags = KEYEVENTF_UNICODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof(INPUT));
}

void KeyboardDevice::setHeld(const std::string& keyName, bool down) {
    auto parts = splitKeyCombo(keyName);
    if (parts.size() > 1) {
        for (const auto& p : parts) setHeld(p, down);
        return;
    }
    std::string key = parts.empty() ? canonicalize(keyName) : parts[0];
    wchar_t ch = 0;
    WORD scan = 0;
    if (!map_.count(key) && !parseUnicodeName(key, ch) && !parseScanName(key, scan) && !appCommandFor(key))
        return;
    if (down) desiredHeld_.insert(key);
    else desiredHeld_.erase(key);
}

void KeyboardDevice::press(const std::string& keyName) {
    auto parts = splitKeyCombo(keyName);
    if (parts.empty()) return;
    for (const auto& p : parts) sendNamed(p, true);
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) sendNamed(*it, false);
}

void KeyboardDevice::release(const std::string& keyName) {
    auto parts = splitKeyCombo(keyName);
    for (const auto& p : parts) {
        unsigned short vk = vkFor(p);
        desiredHeld_.erase(p);
        if (vk) sendVk(vk, false);
        physicallyHeld_.erase(p);
    }
}

void KeyboardDevice::releaseAllHeld() { desiredHeld_.clear(); }

void KeyboardDevice::typeText(const std::string& text) {
    if (dryRun) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    for (wchar_t ch : w) {
        sendUnicode(ch, true);
        sendUnicode(ch, false);
    }
}

void KeyboardDevice::setProperty(const std::string& name, const Value& value) {
    std::string key = canonicalize(name);
    if (key == "repeatfakekeys") repeatFakeKeys_ = value.isTruthy();
    else if (key == "repeatmultiplefakekeys") repeatMultipleFakeKeys_ = value.isTruthy();
    else if (key == "repeatrate") repeatRateHz_ = value.asNumber();
    else if (key == "repeatdelay") repeatDelaySec_ = value.asNumber();
    else setHeld(name, value.isTruthy());
}

void KeyboardDevice::swallow(const std::string& keyName, bool on) {
    auto parts = splitKeyCombo(keyName);
    for (const auto& p : parts) {
        unsigned short vk = vkFor(p);
        if (!vk) continue;
        if (on) swallowedVk_.insert(vk);
        else swallowedVk_.erase(vk);
    }
    if (!swallowedVk_.empty()) installSwallowHook();
    else removeSwallowHook();
}

bool KeyboardDevice::isSwallowedVk(unsigned long vk) const {
    return swallowedVk_.count(static_cast<unsigned short>(vk)) > 0;
}

int KeyboardDevice::keyboardCount() const {
    UINT n = 0;
    GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST));
    if (n == 0) return 1;
    std::vector<RAWINPUTDEVICELIST> list(n);
    if (GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) return 1;
    int count = 0;
    for (UINT i = 0; i < n; ++i)
        if (list[i].dwType == RIM_TYPEKEYBOARD) ++count;
    return std::max(1, count);
}

bool KeyboardDevice::anyKeyDown() const {
    if (!desiredHeld_.empty()) return true;
    if (dryRun) return false;
    for (const auto& kv : map_) {
        if ((GetAsyncKeyState(kv.second) & 0x8000) != 0) return true;
    }
    return false;
}

void KeyboardDevice::installSwallowHook() {
    if (dryRun || swallowHook_) return;
    g_kbHookOwner = this;
    swallowHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardSwallowProc, GetModuleHandleW(nullptr), 0);
}

void KeyboardDevice::removeSwallowHook() {
    if (swallowHook_) {
        UnhookWindowsHookEx(static_cast<HHOOK>(swallowHook_));
        swallowHook_ = nullptr;
    }
    if (g_kbHookOwner == this) g_kbHookOwner = nullptr;
}

void KeyboardDevice::reset() {
    removeSwallowHook();
    desiredHeld_.clear();
    physicallyHeld_.clear();
    swallowedVk_.clear();
}

void KeyboardDevice::endFrame() {
    for (const auto& key : desiredHeld_) {
        if (!physicallyHeld_.count(key)) {
            sendNamed(key, true);
            physicallyHeld_.insert(key);
        }
    }
    std::vector<std::string> toRelease;
    for (const auto& key : physicallyHeld_) {
        if (!desiredHeld_.count(key)) toRelease.push_back(key);
    }
    for (const auto& key : toRelease) {
        sendNamed(key, false);
        physicallyHeld_.erase(key);
    }
}
