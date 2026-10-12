#define INITGUID
#define DIRECTINPUT_VERSION 0x0800

#include "GameControllers.h"

#include <Windows.h>
#include <winioctl.h>
#include <dinput.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <xinput.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string canon(std::string s) {
    s = lower(std::move(s));
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    return s;
}

double clamp1(double v) { return std::clamp(v, -1.0, 1.0); }

bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

int suffixIndex(const std::string& s, const char* prefix, int fallback = 1) {
    size_t n = std::strlen(prefix);
    if (!startsWith(s, prefix)) return fallback;
    if (s.size() == n) return fallback;
    if (!std::isdigit(static_cast<unsigned char>(s[n]))) return fallback;
    return std::max(1, std::stoi(s.substr(n)));
}

double diAxis(LONG v) { return clamp1((static_cast<double>(v) - 32767.5) / 32767.5); }

double povDegrees(DWORD pov) {
    if (pov == 0xFFFFFFFF || LOWORD(pov) == 0xFFFF) return -1;
    return (pov % 36000) / 100.0;
}

bool povDir(DWORD pov, const char* dir) {
    double d = povDegrees(pov);
    if (d < 0) return false;
    if (std::strcmp(dir, "up") == 0) return d >= 315 || d <= 45;
    if (std::strcmp(dir, "right") == 0) return d >= 45 && d <= 135;
    if (std::strcmp(dir, "down") == 0) return d >= 135 && d <= 225;
    if (std::strcmp(dir, "left") == 0) return d >= 225 && d <= 315;
    return false;
}

double povX(DWORD pov) {
    if (povDir(pov, "right")) return 1;
    if (povDir(pov, "left")) return -1;
    return 0;
}
double povY(DWORD pov) {
    if (povDir(pov, "down")) return 1;
    if (povDir(pov, "up")) return -1;
    return 0;
}

BOOL CALLBACK enumJoysticks(const DIDEVICEINSTANCE* inst, VOID* ctx) {
    auto* list = static_cast<std::vector<DIDEVICEINSTANCE>*>(ctx);
    list->push_back(*inst);
    return DIENUM_CONTINUE;
}

const unsigned short kWiiButtonsLeft = 0x0001;
const unsigned short kWiiButtonsRight = 0x0002;
const unsigned short kWiiButtonsDown = 0x0004;
const unsigned short kWiiButtonsUp = 0x0008;
const unsigned short kWiiButtonsPlus = 0x0010;
const unsigned short kWiiButtonsTwo = 0x0100;
const unsigned short kWiiButtonsOne = 0x0200;
const unsigned short kWiiButtonsB = 0x0400;
const unsigned short kWiiButtonsA = 0x0800;
const unsigned short kWiiButtonsMinus = 0x1000;
const unsigned short kWiiButtonsHome = 0x8000;
}

struct GameControllers::Impl {
    IDirectInput8* di = nullptr;
    struct Pad {
        DIDEVICEINSTANCE inst{};
        IDirectInputDevice8* dev = nullptr;
        DIJOYSTATE2 state{};
        bool acquired = false;
        bool attached = false;
        DWORD devType = 0;
        std::string name;
        double vibe1 = 0;
        double vibe2 = 0;
    };
    std::vector<Pad> pads;

    struct XPad {
        bool exists = false;
        XINPUT_STATE state{};
        double vibe1 = 0;
        double vibe2 = 0;
    };
    XPad xpad[4];

    struct PpJoy {
        double analog[16]{};
        bool digital[32]{};
    };
    PpJoy ppjoy[16];

    struct Wii {
        HANDLE hid = INVALID_HANDLE_VALUE;
        bool exists = false;
        unsigned short buttons = 0;
        double gx = 0, gy = 0, gz = 0;
        bool rumble = false;
        bool led[4]{};
        bool hasNunchuk = false;
        double joyX = 0, joyY = 0;
        bool c = false, z = false;
        bool overlayA = false, overlayB = false;
        struct IrDot {
            double x = 0;
            double y = 0;
            bool vis = false;
            int size = 0;
        };
        IrDot dots[4]{};
        double pointerX = 0;
        double pointerY = 0;
        bool pointerVisible = false;
        bool hasClassic = false;
        bool hasGuitar = false;
        bool hasBalance = false;
        bool clA = false, clB = false, clX = false, clY = false;
        bool clPlus = false, clMinus = false, clHome = false;
        bool clUp = false, clDown = false, clLeft = false, clRight = false;
        bool clL = false, clR = false, clZL = false, clZR = false;
        double clJoy1x = 0, clJoy1y = 0, clJoy2x = 0, clJoy2y = 0;
        bool fret[5]{};
        bool strumUp = false, strumDown = false;
        double whammy = 0;
        double balanceKg[4]{};
        double weight = 0;
        double frequency = 0;
        bool speaker = false;
        bool mute = false;
    };
    std::vector<Wii> wiis;

    bool inited = false;

    ~Impl() { shutdown(); }

    void shutdown() {
        for (auto& p : pads) {
            if (p.dev) {
                p.dev->Unacquire();
                p.dev->Release();
                p.dev = nullptr;
            }
        }
        pads.clear();
        if (di) {
            di->Release();
            di = nullptr;
        }
        for (auto& w : wiis) {
            if (w.hid != INVALID_HANDLE_VALUE) {
                CloseHandle(w.hid);
                w.hid = INVALID_HANDLE_VALUE;
            }
        }
        wiis.clear();
        inited = false;
    }

    void ensureDi() {
        if (inited) return;
        inited = true;
        HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8,
                                        reinterpret_cast<void**>(&di), nullptr);
        if (FAILED(hr) || !di) return;
        std::vector<DIDEVICEINSTANCE> found;
        di->EnumDevices(DI8DEVCLASS_GAMECTRL, enumJoysticks, &found, DIEDFL_ATTACHEDONLY);
        for (auto& inst : found) {
            Pad p;
            p.inst = inst;
            p.devType = inst.dwDevType;
            char nameA[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, inst.tszInstanceName, -1, nameA, MAX_PATH, nullptr, nullptr);
            p.name = nameA;
            if (SUCCEEDED(di->CreateDevice(inst.guidInstance, &p.dev, nullptr)) && p.dev) {
                p.dev->SetDataFormat(&c_dfDIJoystick2);
                p.dev->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
                p.acquired = SUCCEEDED(p.dev->Acquire());
                p.attached = true;
            }
            pads.push_back(p);
        }
        openWiimotes();
    }

    static void hidOut(HANDLE h, unsigned char* data, int len) {
        if (h == INVALID_HANDLE_VALUE) return;
        HidD_SetOutputReport(h, data, static_cast<ULONG>(len));
    }

    static void writeMem(HANDLE h, unsigned addr, const unsigned char* data, int len, bool rumble) {
        unsigned char buf[22]{};
        buf[0] = 0x16;
        buf[1] = rumble ? 0x05 : 0x04;
        buf[2] = static_cast<unsigned char>((addr >> 16) & 0xFF);
        buf[3] = static_cast<unsigned char>((addr >> 8) & 0xFF);
        buf[4] = static_cast<unsigned char>(addr & 0xFF);
        buf[5] = static_cast<unsigned char>(len);
        if (data && len > 0) std::memcpy(buf + 6, data, static_cast<size_t>(std::min(len, 16)));
        hidOut(h, buf, 22);
    }

    static void updatePointer(Wii& w) {
        int n = 0;
        double sx = 0, sy = 0;
        for (auto& d : w.dots) {
            if (!d.vis) continue;
            sx += d.x;
            sy += d.y;
            ++n;
        }
        w.pointerVisible = n >= 1;
        if (n >= 1) {
            w.pointerX = std::clamp(1.0 - sx / n, 0.0, 1.0);
            w.pointerY = std::clamp(sy / n, 0.0, 1.0);
        } else {
            w.pointerX = 0;
            w.pointerY = 0;
        }
    }

    static void parseIr10(const unsigned char* d, Wii& w) {
        for (int pair = 0; pair < 2; ++pair) {
            const unsigned char* p = d + pair * 5;
            int xs[2] = {p[0] | ((p[2] & 0x30) << 4), p[3] | ((p[2] & 0x03) << 8)};
            int ys[2] = {p[1] | ((p[2] & 0xC0) << 2), p[4] | ((p[2] & 0x0C) << 6)};
            for (int k = 0; k < 2; ++k) {
                Wii::IrDot& dot = w.dots[pair * 2 + k];
                bool vis = xs[k] != 0x3FF && ys[k] != 0x3FF;
                dot.vis = vis;
                dot.x = vis ? xs[k] / 1023.0 : 0;
                dot.y = vis ? ys[k] / 767.0 : 0;
                dot.size = vis ? 16 : 0;
            }
        }
        updatePointer(w);
    }

    static void parseIr12(const unsigned char* d, Wii& w) {
        for (int i = 0; i < 4; ++i) {
            const unsigned char* p = d + i * 3;
            int x = p[0] | ((p[2] & 0x30) << 4);
            int y = p[1] | ((p[2] & 0xC0) << 2);
            Wii::IrDot& dot = w.dots[i];
            bool vis = x != 0x3FF && y != 0x3FF;
            dot.vis = vis;
            dot.x = vis ? x / 1023.0 : 0;
            dot.y = vis ? y / 767.0 : 0;
            dot.size = vis ? (p[2] & 0x0F) : 0;
        }
        updatePointer(w);
    }

    static void parseNunchuk(const unsigned char* ext, Wii& w) {
        bool empty = true;
        for (int i = 0; i < 6; ++i) {
            if (ext[i] != 0xFF) empty = false;
        }
        if (empty) {
            w.hasNunchuk = false;
            w.joyX = w.joyY = 0;
            w.c = w.z = false;
            return;
        }
        w.hasNunchuk = true;
        w.joyX = clamp1((static_cast<int>(ext[0]) - 128) / 96.0);
        // GlovePIE: JoyY is -1 up, +1 down.
        w.joyY = clamp1((128 - static_cast<int>(ext[1])) / 96.0);
        w.z = (ext[5] & 0x01) == 0;
        w.c = (ext[5] & 0x02) == 0;
    }

    static void parseClassic(const unsigned char* ext, Wii& w) {
        w.hasClassic = true;
        w.clJoy1x = clamp1(((ext[0] & 0x3F) - 32) / 32.0);
        w.clJoy1y = clamp1((32 - (ext[1] & 0x3F)) / 32.0);
        int rx = ((ext[0] & 0xC0) >> 3) | ((ext[1] & 0xC0) >> 5) | ((ext[2] & 0x80) >> 7);
        w.clJoy2x = clamp1((rx - 16) / 16.0);
        w.clJoy2y = clamp1((16 - (ext[2] & 0x1F)) / 16.0);
        unsigned b4 = ext[4], b5 = ext[5];
        w.clR = (b4 & 0x02) == 0;
        w.clPlus = (b4 & 0x04) == 0;
        w.clHome = (b4 & 0x08) == 0;
        w.clMinus = (b4 & 0x10) == 0;
        w.clL = (b4 & 0x20) == 0;
        w.clDown = (b4 & 0x40) == 0;
        w.clRight = (b4 & 0x80) == 0;
        w.clUp = (b5 & 0x01) == 0;
        w.clLeft = (b5 & 0x02) == 0;
        w.clZR = (b5 & 0x04) == 0;
        w.clX = (b5 & 0x08) == 0;
        w.clA = (b5 & 0x10) == 0;
        w.clY = (b5 & 0x20) == 0;
        w.clB = (b5 & 0x40) == 0;
        w.clZL = (b5 & 0x80) == 0;
        w.hasGuitar = w.hasClassic;
        w.fret[0] = w.clY;
        w.fret[1] = w.clX;
        w.fret[2] = w.clB;
        w.fret[3] = w.clA;
        w.fret[4] = w.clL;
        w.strumUp = w.clUp;
        w.strumDown = w.clDown;
        w.whammy = (w.clJoy2x + 1) * 0.5;
    }

    void initWiimote(Wii& w) {
        if (w.hid == INVALID_HANDLE_VALUE) return;
        unsigned char n55 = 0x55, n00 = 0x00;
        writeMem(w.hid, 0xA400F0, &n55, 1, w.rumble);
        Sleep(10);
        writeMem(w.hid, 0xA400FB, &n00, 1, w.rumble);
        Sleep(10);
        unsigned char irOn[2] = {0x13, static_cast<unsigned char>(w.rumble ? 0x05 : 0x04)};
        unsigned char irOn2[2] = {0x1A, static_cast<unsigned char>(w.rumble ? 0x05 : 0x04)};
        hidOut(w.hid, irOn, 2);
        hidOut(w.hid, irOn2, 2);
        Sleep(10);
        unsigned char eight = 0x08;
        writeMem(w.hid, 0xB00030, &eight, 1, w.rumble);
        Sleep(10);
        unsigned char sens1[] = {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x64, 0x00, 0xfe};
        writeMem(w.hid, 0xB00000, sens1, 9, w.rumble);
        Sleep(10);
        unsigned char sens2[] = {0xfd, 0x05};
        writeMem(w.hid, 0xB0001A, sens2, 2, w.rumble);
        Sleep(10);
        unsigned char irMode = 0x01; // basic IR (10 bytes) for report 0x37
        writeMem(w.hid, 0xB00033, &irMode, 1, w.rumble);
        Sleep(10);
        writeMem(w.hid, 0xB00030, &eight, 1, w.rumble);
        Sleep(10);
        unsigned char rpt[3] = {0x12, static_cast<unsigned char>(w.rumble ? 0x05 : 0x04), 0x37};
        hidOut(w.hid, rpt, 3);
    }

    void openWiimotes() {
        GUID hidGuid;
        HidD_GetHidGuid(&hidGuid);
        HDEVINFO info = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (info == INVALID_HANDLE_VALUE) return;
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hidGuid, i, &iface); ++i) {
            DWORD need = 0;
            SetupDiGetDeviceInterfaceDetailW(info, &iface, nullptr, 0, &need, nullptr);
            if (need == 0) continue;
            std::vector<char> buf(need);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(info, &iface, detail, need, nullptr, nullptr)) continue;
            HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED, nullptr);
            if (h == INVALID_HANDLE_VALUE) continue;
            HIDD_ATTRIBUTES attr{};
            attr.Size = sizeof(attr);
            if (!HidD_GetAttributes(h, &attr) || attr.VendorID != 0x057E ||
                (attr.ProductID != 0x0306 && attr.ProductID != 0x0330)) {
                CloseHandle(h);
                continue;
            }
            Wii w;
            w.hid = h;
            w.exists = true;
            initWiimote(w);
            wiis.push_back(w);
            if (wiis.size() >= 8) break;
        }
        SetupDiDestroyDeviceInfoList(info);
    }

    void pollWiimotes() {
        for (auto& w : wiis) {
            if (w.hid == INVALID_HANDLE_VALUE) continue;
            unsigned char buf[22]{};
            DWORD read = 0;
            OVERLAPPED ov{};
            ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            BOOL ok = ReadFile(w.hid, buf, sizeof(buf), &read, &ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(ov.hEvent, 0) == WAIT_OBJECT_0)
                    GetOverlappedResult(w.hid, &ov, &read, FALSE);
                else
                    CancelIo(w.hid);
            }
            CloseHandle(ov.hEvent);
            if (read < 3) continue;
            unsigned char id = buf[0];
            int off = (id >= 0x30 && id <= 0x37) ? 1 : 0;
            if (off == 0) id = 0x30;
            w.buttons = static_cast<unsigned short>(buf[off] | (buf[off + 1] << 8));
            if (id == 0x31 || id == 0x33 || id == 0x35 || id == 0x37) {
                if (read >= static_cast<DWORD>(off + 5)) {
                    w.gx = (static_cast<int>(buf[off + 2]) - 0x80) / 25.0;
                    w.gy = (static_cast<int>(buf[off + 3]) - 0x80) / 25.0;
                    w.gz = (static_cast<int>(buf[off + 4]) - 0x80) / 25.0;
                }
            }
            if (id == 0x33 && read >= static_cast<DWORD>(off + 17)) parseIr12(buf + off + 5, w);
            if (id == 0x36 && read >= static_cast<DWORD>(off + 14)) parseIr10(buf + off + 2, w);
            if (id == 0x37 && read >= static_cast<DWORD>(off + 15)) parseIr10(buf + off + 5, w);
            if (id == 0x32 && read >= static_cast<DWORD>(off + 10)) {
                parseNunchuk(buf + off + 2, w);
                if (!w.hasNunchuk) parseClassic(buf + off + 2, w);
            }
            if (id == 0x37 && read >= static_cast<DWORD>(off + 21)) {
                parseNunchuk(buf + off + 15, w);
                if (!w.hasNunchuk) parseClassic(buf + off + 15, w);
            }
        }
    }

    void poll() {
        ensureDi();
        for (auto& p : pads) {
            if (!p.dev) continue;
            p.dev->Poll();
            HRESULT hr = p.dev->GetDeviceState(sizeof(DIJOYSTATE2), &p.state);
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                p.dev->Acquire();
                p.dev->GetDeviceState(sizeof(DIJOYSTATE2), &p.state);
            }
            p.attached = SUCCEEDED(hr) || p.acquired;
        }
        for (int i = 0; i < 4; ++i) {
            xpad[i].exists = XInputGetState(i, &xpad[i].state) == ERROR_SUCCESS;
        }
        pollWiimotes();
    }

    Pad* padAt(int index1) {
        ensureDi();
        if (index1 < 1 || index1 > static_cast<int>(pads.size())) return nullptr;
        return &pads[static_cast<size_t>(index1 - 1)];
    }

    Wii* wiiAt(int index1) {
        ensureDi();
        if (index1 < 1) return nullptr;
        if (index1 > static_cast<int>(wiis.size())) {
            // empty slot so scripts can still read false
            static Wii missing;
            missing = Wii{};
            return &missing;
        }
        return &wiis[static_cast<size_t>(index1 - 1)];
    }

    Value joystickGet(int index, std::string rest) {
        rest = canon(rest);
        if (startsWith(rest, "gamepad")) {
            auto dot = rest.find('.');
            rest = dot == std::string::npos ? std::string() : rest.substr(dot + 1);
            index = 1;
        }
        Pad* p = padAt(index);
        auto axis = [&](LONG v) { return p ? diAxis(v) : 0.0; };
        auto btn = [&](int n) {
            if (!p || n < 1 || n > 128) return false;
            return (p->state.rgbButtons[n - 1] & 0x80) != 0;
        };
        DWORD pov = p ? p->state.rgdwPOV[0] : 0xFFFFFFFF;
        if (rest == "exists" || rest == "attached" || rest == "present")
            return Value::boolean(p && p->attached);
        if (rest == "name" || rest == "productname") return Value::string(p ? p->name : "");
        if (rest == "numberofaxes") return Value::number(p ? 6 : 0);
        if (rest == "numberofpovs") return Value::number(p ? 4 : 0);
        if (rest == "x") return Value::number(p ? axis(p->state.lX) : 0);
        if (rest == "y") return Value::number(p ? axis(p->state.lY) : 0);
        if (rest == "z") return Value::number(p ? axis(p->state.lZ) : 0);
        if (rest == "roll" || rest == "rx") return Value::number(p ? axis(p->state.lRx) : 0);
        if (rest == "pitch" || rest == "ry") return Value::number(p ? axis(p->state.lRy) : 0);
        if (rest == "yaw" || rest == "rz") return Value::number(p ? axis(p->state.lRz) : 0);
        if (rest == "slider") return Value::number(p ? axis(p->state.rglSlider[0]) : 0);
        if (rest == "dial") return Value::number(p ? axis(p->state.rglSlider[1]) : 0);
        if (rest == "pos2d")
            return Value::vector({p ? axis(p->state.lX) : 0, p ? axis(p->state.lY) : 0});
        if (rest == "pos" || rest == "pos3d")
            return Value::vector({p ? axis(p->state.lX) : 0, p ? axis(p->state.lY) : 0, p ? axis(p->state.lZ) : 0});
        if (rest == "pos2d2")
            return Value::vector({p ? axis(p->state.lRx) : 0, p ? axis(p->state.lZ) : 0});
        if (rest == "vibration1") return Value::number(p ? p->vibe1 : 0);
        if (rest == "vibration2") return Value::number(p ? p->vibe2 : 0);
        if (startsWith(rest, "button")) {
            int n = suffixIndex(rest, "button", 1);
            return Value::boolean(btn(n));
        }
        if (startsWith(rest, "pov")) {
            int n = 1;
            size_t i = 3;
            if (i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i]))) {
                n = rest[i] - '0';
                rest = rest.substr(i + 1);
            } else
                rest = rest.substr(3);
            DWORD pv = p && n >= 1 && n <= 4 ? p->state.rgdwPOV[n - 1] : 0xFFFFFFFF;
            if (rest.empty()) return Value::number(povDegrees(pv));
            if (rest == "center" || rest == "centred" || rest == "centered")
                return Value::boolean(povDegrees(pv) < 0);
            if (rest == "up" || rest == "down" || rest == "left" || rest == "right")
                return Value::boolean(povDir(pv, rest.c_str()));
            if (rest == "x") return Value::number(povX(pv));
            if (rest == "y") return Value::number(povY(pv));
        }
        if (rest == "up") return Value::boolean(povDir(pov, "up") || (p && axis(p->state.lY) < -0.5));
        if (rest == "down") return Value::boolean(povDir(pov, "down") || (p && axis(p->state.lY) > 0.5));
        if (rest == "left") return Value::boolean(povDir(pov, "left") || (p && axis(p->state.lX) < -0.5));
        if (rest == "right") return Value::boolean(povDir(pov, "right") || (p && axis(p->state.lX) > 0.5));
        if (rest == "cross") return Value::boolean(btn(1));
        if (rest == "circle") return Value::boolean(btn(2));
        if (rest == "square") return Value::boolean(btn(3));
        if (rest == "triangle") return Value::boolean(btn(4));
        if (rest == "l1") return Value::boolean(btn(5));
        if (rest == "r1") return Value::boolean(btn(6));
        if (rest == "l2") return Value::boolean(btn(7));
        if (rest == "r2") return Value::boolean(btn(8));
        if (rest == "select") return Value::boolean(btn(9));
        if (rest == "start") return Value::boolean(btn(10));
        if (rest == "ps") return Value::boolean(btn(13));
        if (rest == "gyro" || rest == "yaw") return Value::number(p ? axis(p->state.lRz) : 0);
        return Value::boolean(false);
    }

    void joystickSet(int index, std::string rest, const Value& v) {
        rest = canon(rest);
        Pad* p = padAt(index);
        if (!p) return;
        if (rest == "vibration1") p->vibe1 = clamp1(v.asNumber());
        if (rest == "vibration2") p->vibe2 = clamp1(v.asNumber());
    }

    Value xinputGet(int index, std::string rest) {
        rest = canon(rest);
        if (rest == "count") {
            int n = 0;
            for (int i = 0; i < 4; ++i)
                if (xpad[i].exists) ++n;
            return Value::number(n);
        }
        if (index < 1 || index > 4) return Value::boolean(false);
        XPad& p = xpad[index - 1];
        auto& g = p.state.Gamepad;
        auto bit = [&](int b) { return p.exists && (g.wButtons & b) != 0; };
        if (rest == "exists" || rest == "present" || rest == "attached") return Value::boolean(p.exists);
        if (rest == "a") return Value::boolean(bit(XINPUT_GAMEPAD_A));
        if (rest == "b") return Value::boolean(bit(XINPUT_GAMEPAD_B));
        if (rest == "x") return Value::boolean(bit(XINPUT_GAMEPAD_X));
        if (rest == "y") return Value::boolean(bit(XINPUT_GAMEPAD_Y));
        if (rest == "back") return Value::boolean(bit(XINPUT_GAMEPAD_BACK));
        if (rest == "start") return Value::boolean(bit(XINPUT_GAMEPAD_START));
        if (rest == "up") return Value::boolean(bit(XINPUT_GAMEPAD_DPAD_UP));
        if (rest == "down") return Value::boolean(bit(XINPUT_GAMEPAD_DPAD_DOWN));
        if (rest == "left") return Value::boolean(bit(XINPUT_GAMEPAD_DPAD_LEFT));
        if (rest == "right") return Value::boolean(bit(XINPUT_GAMEPAD_DPAD_RIGHT));
        if (rest == "leftshoulder") return Value::boolean(bit(XINPUT_GAMEPAD_LEFT_SHOULDER));
        if (rest == "rightshoulder") return Value::boolean(bit(XINPUT_GAMEPAD_RIGHT_SHOULDER));
        if (rest == "leftthumb") return Value::boolean(bit(XINPUT_GAMEPAD_LEFT_THUMB));
        if (rest == "rightthumb") return Value::boolean(bit(XINPUT_GAMEPAD_RIGHT_THUMB));
        if (rest == "lefttrigger") return Value::number(p.exists ? g.bLeftTrigger / 255.0 : 0);
        if (rest == "righttrigger") return Value::number(p.exists ? g.bRightTrigger / 255.0 : 0);
        auto stick = [](SHORT v) { return clamp1(v / 32767.0); };
        if (rest == "joy1x") return Value::number(p.exists ? stick(g.sThumbLX) : 0);
        if (rest == "joy1y") return Value::number(p.exists ? -stick(g.sThumbLY) : 0);
        if (rest == "joy2x") return Value::number(p.exists ? stick(g.sThumbRX) : 0);
        if (rest == "joy2y") return Value::number(p.exists ? -stick(g.sThumbRY) : 0);
        if (rest == "joy1")
            return Value::vector({p.exists ? stick(g.sThumbLX) : 0, p.exists ? -stick(g.sThumbLY) : 0});
        if (rest == "joy2")
            return Value::vector({p.exists ? stick(g.sThumbRX) : 0, p.exists ? -stick(g.sThumbRY) : 0});
        if (rest == "vibration1") return Value::number(p.vibe1);
        if (rest == "vibration2") return Value::number(p.vibe2);
        return Value::boolean(false);
    }

    void xinputSet(int index, std::string rest, const Value& v) {
        rest = canon(rest);
        if (index < 1 || index > 4) return;
        XPad& p = xpad[index - 1];
        if (rest == "vibration1") p.vibe1 = std::clamp(v.asNumber(), 0.0, 1.0);
        if (rest == "vibration2") p.vibe2 = std::clamp(v.asNumber(), 0.0, 1.0);
        if (!dryRun_ && p.exists && (rest == "vibration1" || rest == "vibration2")) {
            XINPUT_VIBRATION vib{};
            vib.wLeftMotorSpeed = static_cast<WORD>(p.vibe1 * 65535);
            vib.wRightMotorSpeed = static_cast<WORD>(p.vibe2 * 65535);
            XInputSetState(index - 1, &vib);
        }
    }

    bool dryRun_ = false;

    Value ppjoyGet(int index, std::string rest) {
        rest = canon(rest);
        if (index < 1 || index > 16) return Value::boolean(false);
        PpJoy& p = ppjoy[index - 1];
        if (startsWith(rest, "analog")) {
            int n = suffixIndex(rest, "analog", 0);
            if (n >= 0 && n < 16) return Value::number(p.analog[n]);
        }
        if (startsWith(rest, "digital")) {
            int n = suffixIndex(rest, "digital", 0);
            if (n >= 0 && n < 32) return Value::boolean(p.digital[n]);
        }
        if (rest == "x") return Value::number(p.analog[0]);
        if (rest == "y") return Value::number(p.analog[1]);
        if (rest == "z") return Value::number(p.analog[2]);
        return Value::boolean(false);
    }

    void ppjoySet(int index, std::string rest, const Value& v) {
        rest = canon(rest);
        if (index < 1 || index > 16) return;
        PpJoy& p = ppjoy[index - 1];
        if (startsWith(rest, "analog")) {
            int n = suffixIndex(rest, "analog", 0);
            if (n >= 0 && n < 16) p.analog[n] = clamp1(v.asNumber());
        }
        if (startsWith(rest, "digital")) {
            int n = suffixIndex(rest, "digital", 0);
            if (n >= 0 && n < 32) p.digital[n] = v.isTruthy();
        }
        if (rest == "x") p.analog[0] = clamp1(v.asNumber());
        if (rest == "y") p.analog[1] = clamp1(v.asNumber());
        if (rest == "z") p.analog[2] = clamp1(v.asNumber());
        if (dryRun_) return;
        wchar_t path[64]{};
        swprintf(path, 64, L"\\\\.\\PPJoyIOCTL%d", index);
        HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        struct {
            unsigned long signature;
            unsigned char numAnalog;
            unsigned char analog[16];
            unsigned char numDigital;
            unsigned char digital[16];
        } st{};
        st.signature = 0x53544154;
        st.numAnalog = 16;
        st.numDigital = 16;
        for (int i = 0; i < 16; ++i)
            st.analog[i] = static_cast<unsigned char>(std::clamp((p.analog[i] + 1.0) * 127.5, 1.0, 255.0));
        for (int i = 0; i < 16; ++i) st.digital[i] = p.digital[i] ? 1 : 0;
        DWORD out = 0;
        DeviceIoControl(h, CTL_CODE(FILE_DEVICE_UNKNOWN, 0x010, METHOD_BUFFERED, FILE_WRITE_ACCESS), &st,
                        sizeof(st), nullptr, 0, &out, nullptr);
        CloseHandle(h);
    }

    Value wiiGet(int index, std::string rest) {
        rest = canon(rest);
        Wii* w = wiiAt(index);
        auto down = [&](unsigned short mask) { return w && ((w->buttons & mask) || false); };
        if (startsWith(rest, "nunchuk") || startsWith(rest, "nunchuck")) {
            auto dot = rest.find('.');
            std::string n = dot == std::string::npos ? std::string() : rest.substr(dot + 1);
            if (n == "joyx") return Value::number(w ? w->joyX : 0);
            if (n == "joyy") return Value::number(w ? w->joyY : 0);
            if (n == "joy") return Value::vector({w ? w->joyX : 0, w ? w->joyY : 0});
            if (n == "cbutton" || n == "c") return Value::boolean(w && w->c);
            if (n == "zbutton" || n == "z") return Value::boolean(w && w->z);
            if (n == "exists") return Value::boolean(w && w->hasNunchuk);
            return Value::boolean(false);
        }
        if (rest == "exists" || rest == "present" || rest == "attached") return Value::boolean(w && w->exists);
        if (rest == "a") return Value::boolean(down(kWiiButtonsA) || (w && w->overlayA));
        if (rest == "b") return Value::boolean(down(kWiiButtonsB) || (w && w->overlayB));
        if (rest == "one" || rest == "1") return Value::boolean(down(kWiiButtonsOne));
        if (rest == "two" || rest == "2") return Value::boolean(down(kWiiButtonsTwo));
        if (rest == "plus") return Value::boolean(down(kWiiButtonsPlus));
        if (rest == "minus") return Value::boolean(down(kWiiButtonsMinus));
        if (rest == "home") return Value::boolean(down(kWiiButtonsHome));
        if (rest == "up") return Value::boolean(down(kWiiButtonsUp));
        if (rest == "down") return Value::boolean(down(kWiiButtonsDown));
        if (rest == "left") return Value::boolean(down(kWiiButtonsLeft));
        if (rest == "right") return Value::boolean(down(kWiiButtonsRight));
        if (rest == "gx") return Value::number(w ? w->gx : 0);
        if (rest == "gy") return Value::number(w ? w->gy : 0);
        if (rest == "gz") return Value::number(w ? w->gz : 0);
        if (rest == "g") return Value::vector({w ? w->gx : 0, w ? w->gy : 0, w ? w->gz : 0});
        if (rest == "pitch" || rest == "smoothpitch") {
            if (!w) return Value::number(0);
            return Value::number(std::atan2(-w->gz, w->gy) * 180.0 / 3.14159265358979323846);
        }
        if (rest == "roll" || rest == "smoothroll") {
            if (!w) return Value::number(0);
            return Value::number(std::atan2(w->gx, w->gy) * 180.0 / 3.14159265358979323846);
        }
        if (rest == "rumble") return Value::boolean(w && w->rumble);
        if (rest == "hasnunchuck" || rest == "hasnunchuk") return Value::boolean(w && w->hasNunchuk);
        if (rest == "hasclassic") return Value::boolean(w && w->hasClassic);
        if (rest == "hasguitar") return Value::boolean(w && w->hasGuitar);
        if (rest == "hasbalanceboard") return Value::boolean(w && w->hasBalance);
        if (rest == "frequency") return Value::number(w ? w->frequency : 0, "hz");
        if (rest == "speaker") return Value::boolean(w && w->speaker);
        if (rest == "mute") return Value::boolean(w && w->mute);
        if (startsWith(rest, "classic")) {
            auto dot = rest.find('.');
            std::string n = dot == std::string::npos ? std::string() : rest.substr(dot + 1);
            if (!w) return Value::boolean(false);
            if (n == "a") return Value::boolean(w->clA);
            if (n == "b") return Value::boolean(w->clB);
            if (n == "x") return Value::boolean(w->clX);
            if (n == "y") return Value::boolean(w->clY);
            if (n == "plus") return Value::boolean(w->clPlus);
            if (n == "minus") return Value::boolean(w->clMinus);
            if (n == "home") return Value::boolean(w->clHome);
            if (n == "up") return Value::boolean(w->clUp);
            if (n == "down") return Value::boolean(w->clDown);
            if (n == "left") return Value::boolean(w->clLeft);
            if (n == "right") return Value::boolean(w->clRight);
            if (n == "l" || n == "lfull") return Value::boolean(w->clL);
            if (n == "r" || n == "rfull") return Value::boolean(w->clR);
            if (n == "zl") return Value::boolean(w->clZL);
            if (n == "zr") return Value::boolean(w->clZR);
            if (n == "joy1x") return Value::number(w->clJoy1x);
            if (n == "joy1y") return Value::number(w->clJoy1y);
            if (n == "joy2x") return Value::number(w->clJoy2x);
            if (n == "joy2y") return Value::number(w->clJoy2y);
            return Value::boolean(false);
        }
        if (startsWith(rest, "guitar")) {
            auto dot = rest.find('.');
            std::string n = dot == std::string::npos ? std::string() : rest.substr(dot + 1);
            if (!w) return Value::boolean(false);
            if (startsWith(n, "fret")) {
                int i = suffixIndex(n, "fret", 1);
                return Value::boolean(i >= 1 && i <= 5 && w->fret[i - 1]);
            }
            if (n == "plus") return Value::boolean(w->clPlus);
            if (n == "minus") return Value::boolean(w->clMinus);
            if (n == "joyx") return Value::number(w->clJoy1x);
            if (n == "joyy") return Value::number(w->clJoy1y);
            if (n == "whammy") return Value::number(w->whammy);
            if (n == "strumup") return Value::boolean(w->strumUp);
            if (n == "strumdown") return Value::boolean(w->strumDown);
            return Value::boolean(false);
        }
        if (startsWith(rest, "balance")) {
            if (!w) return Value::number(0);
            if (rest == "balanceboard.weight" || rest == "weight") return Value::number(w->weight);
            return Value::number(w->weight);
        }
        if (rest == "led1") return Value::boolean(w && w->led[0]);
        if (rest == "led2") return Value::boolean(w && w->led[1]);
        if (rest == "led3") return Value::boolean(w && w->led[2]);
        if (rest == "led4") return Value::boolean(w && w->led[3]);
        if (rest == "leds") {
            int n = 0;
            if (w)
                for (int i = 0; i < 4; ++i)
                    if (w->led[i]) n |= (1 << i);
            return Value::number(n);
        }
        if (rest == "pointerx" || rest == "irx") return Value::number(w ? w->pointerX : 0);
        if (rest == "pointery" || rest == "iry") return Value::number(w ? w->pointerY : 0);
        if (rest == "pointervisible" || rest == "pointerfound" || rest == "irvisible")
            return Value::boolean(w && w->pointerVisible);
        if (startsWith(rest, "dot")) {
            int n = 1;
            size_t i = 3;
            if (i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i]))) {
                n = rest[i] - '0';
                ++i;
            }
            std::string suf = i < rest.size() ? rest.substr(i) : std::string();
            if (n < 1 || n > 4) return Value::boolean(false);
            const Wii::IrDot empty{};
            const Wii::IrDot& d = w ? w->dots[n - 1] : empty;
            if (suf.empty() || suf == "x") return Value::number(d.x);
            if (suf == "y") return Value::number(d.y);
            if (suf == "vis" || suf == "visible" || suf == "found") return Value::boolean(d.vis);
            if (suf == "size") return Value::number(d.size);
            return Value::number(0);
        }
        return Value::boolean(false);
    }

    void wiiSet(int index, std::string rest, const Value& v) {
        rest = canon(rest);
        ensureDi();
        if (index < 1) return;
        if (index > static_cast<int>(wiis.size())) {
            wiis.resize(static_cast<size_t>(index));
        }
        Wii& w = wiis[static_cast<size_t>(index - 1)];
        if (rest == "a") w.overlayA = v.isTruthy();
        if (rest == "b") w.overlayB = v.isTruthy();
        if (rest == "rumble") w.rumble = v.isTruthy();
        if (rest == "led1") w.led[0] = v.isTruthy();
        if (rest == "led2") w.led[1] = v.isTruthy();
        if (rest == "led3") w.led[2] = v.isTruthy();
        if (rest == "led4") w.led[3] = v.isTruthy();
        if (rest == "leds") {
            int n = static_cast<int>(v.asNumber());
            for (int i = 0; i < 4; ++i) w.led[i] = (n & (1 << i)) != 0;
        }
        if (rest == "frequency") w.frequency = v.asNumber();
        if (rest == "speaker") w.speaker = v.isTruthy();
        if (rest == "mute") w.mute = v.isTruthy();
        if (w.hid != INVALID_HANDLE_VALUE && !dryRun_) {
            unsigned char leds = 0;
            if (w.led[0]) leds |= 0x10;
            if (w.led[1]) leds |= 0x20;
            if (w.led[2]) leds |= 0x40;
            if (w.led[3]) leds |= 0x80;
            if (w.rumble) leds |= 0x01;
            unsigned char rpt[2] = {0x11, leds};
            HidD_SetOutputReport(w.hid, rpt, sizeof(rpt));
        }
    }

    Value anyGet(std::string rest) {
        rest = canon(rest);
        auto firstX = [&](const char* n) { return xinputGet(1, n); };
        auto firstJ = [&](const char* n) { return joystickGet(1, n); };
        auto firstW = [&](const char* n) { return wiiGet(1, n); };
        if (rest == "joy1x") {
            if (xpad[0].exists) return xinputGet(1, "joy1x");
            return joystickGet(1, "x");
        }
        if (rest == "joy1y") {
            if (xpad[0].exists) return xinputGet(1, "joy1y");
            return joystickGet(1, "y");
        }
        if (rest == "joy2x") {
            if (xpad[0].exists) return xinputGet(1, "joy2x");
            return joystickGet(1, "roll");
        }
        if (rest == "joy2y") {
            if (xpad[0].exists) return xinputGet(1, "joy2y");
            return joystickGet(1, "z");
        }
        if (rest == "dpadleft" || rest == "left")
            return Value::boolean(firstX("left").isTruthy() || firstJ("left").isTruthy() || firstW("left").isTruthy());
        if (rest == "dpadright" || rest == "right")
            return Value::boolean(firstX("right").isTruthy() || firstJ("right").isTruthy() || firstW("right").isTruthy());
        if (rest == "dpadup" || rest == "up")
            return Value::boolean(firstX("up").isTruthy() || firstJ("up").isTruthy() || firstW("up").isTruthy());
        if (rest == "dpaddown" || rest == "down")
            return Value::boolean(firstX("down").isTruthy() || firstJ("down").isTruthy() || firstW("down").isTruthy());
        if (rest == "a" || rest == "cross")
            return Value::boolean(firstX("a").isTruthy() || firstJ("button1").isTruthy() || firstW("b").isTruthy());
        if (rest == "b" || rest == "circle")
            return Value::boolean(firstX("b").isTruthy() || firstJ("button2").isTruthy() || firstW("a").isTruthy());
        if (rest == "x" || rest == "square")
            return Value::boolean(firstX("x").isTruthy() || firstJ("button3").isTruthy() || firstW("two").isTruthy());
        if (rest == "y" || rest == "triangle")
            return Value::boolean(firstX("y").isTruthy() || firstJ("button4").isTruthy() || firstW("one").isTruthy());
        if (rest == "start")
            return Value::boolean(firstX("start").isTruthy() || firstW("plus").isTruthy());
        if (rest == "select" || rest == "back")
            return Value::boolean(firstX("back").isTruthy() || firstW("minus").isTruthy());
        if (rest == "home") return firstW("home");
        if (rest == "lefttrigger" || rest == "l2") return firstX("lefttrigger");
        if (rest == "righttrigger" || rest == "r2" || rest == "r1") {
            if (xpad[0].exists) return rest == "r1" ? firstX("rightshoulder") : firstX("righttrigger");
            return firstW("b");
        }
        if (rest == "l1" || rest == "leftshoulder") return firstX("leftshoulder");
        if (rest == "l3") return firstX("leftthumb");
        if (rest == "r3") return firstX("rightthumb");
        if (rest == "vibration1") return xpad[0].exists ? firstX("vibration1") : firstJ("vibration1");
        if (rest == "vibration2") return xpad[0].exists ? firstX("vibration2") : firstJ("vibration2");
        return Value::boolean(false);
    }

    void splitRoot(const std::string& root, std::string& kind, int& index) {
        std::string r = canon(root);
        index = 1;
        if (r == "joy" || r == "joystick") {
            kind = "joystick";
            return;
        }
        if (startsWith(r, "joystick")) {
            kind = "joystick";
            index = suffixIndex(r, "joystick", 1);
            return;
        }
        if (startsWith(r, "joy") && r.size() > 3 && std::isdigit(static_cast<unsigned char>(r[3]))) {
            kind = "joystick";
            index = suffixIndex(r, "joy", 1);
            return;
        }
        if (r == "ppjoy" || startsWith(r, "ppjoy")) {
            kind = "ppjoy";
            index = r == "ppjoy" ? 1 : suffixIndex(r, "ppjoy", 1);
            return;
        }
        if (r == "xinput" || startsWith(r, "xinput") || r == "xbox" || startsWith(r, "xbox")) {
            kind = "xinput";
            if (r == "xinput" || r == "xbox") index = 1;
            else if (startsWith(r, "xinput")) index = suffixIndex(r, "xinput", 1);
            else index = suffixIndex(r, "xbox", 1);
            return;
        }
        if (r == "wiimote" || startsWith(r, "wiimote")) {
            kind = "wiimote";
            index = r == "wiimote" ? 1 : suffixIndex(r, "wiimote", 1);
            return;
        }
        if (r == "nunchuk" || r == "nunchuck") {
            kind = "nunchuk";
            index = 1;
            return;
        }
        if (r == "classic") {
            kind = "classic";
            index = 1;
            return;
        }
        if (r == "guitar" || r == "wiiguitar") {
            kind = "guitar";
            index = 1;
            return;
        }
        if (r == "balanceboard" || r == "balance") {
            kind = "balance";
            index = 1;
            return;
        }
        if (r == "any") {
            kind = "any";
            return;
        }
        if (r == "sixaxis" || startsWith(r, "sixaxis")) {
            kind = "joystick";
            index = 1;
            return;
        }
        kind.clear();
    }
};

GameControllers::GameControllers() : impl_(new Impl) {}
GameControllers::~GameControllers() { delete impl_; }

bool GameControllers::handles(const std::string& root) {
    std::string kind;
    int index = 1;
    impl_->splitRoot(root, kind, index);
    return !kind.empty();
}

Value GameControllers::get(const std::string& root, const std::string& rest) {
    impl_->dryRun_ = dryRun;
    impl_->ensureDi();
    std::string kind;
    int index = 1;
    impl_->splitRoot(root, kind, index);
    if (kind == "joystick") return impl_->joystickGet(index, rest);
    if (kind == "xinput") return impl_->xinputGet(index, rest);
    if (kind == "ppjoy") return impl_->ppjoyGet(index, rest);
    if (kind == "wiimote") return impl_->wiiGet(index, rest);
    if (kind == "nunchuk") return impl_->wiiGet(1, std::string("nunchuk.") + rest);
    if (kind == "classic") return impl_->wiiGet(1, std::string("classic.") + rest);
    if (kind == "guitar") return impl_->wiiGet(1, std::string("guitar.") + rest);
    if (kind == "balance") return impl_->wiiGet(1, std::string("balanceboard.") + rest);
    if (kind == "any") return impl_->anyGet(rest);
    return Value::boolean(false);
}

void GameControllers::set(const std::string& root, const std::string& rest, const Value& value) {
    impl_->dryRun_ = dryRun;
    impl_->ensureDi();
    std::string kind;
    int index = 1;
    impl_->splitRoot(root, kind, index);
    if (kind == "joystick") impl_->joystickSet(index, rest, value);
    else if (kind == "xinput") impl_->xinputSet(index, rest, value);
    else if (kind == "ppjoy") impl_->ppjoySet(index, rest, value);
    else if (kind == "wiimote") impl_->wiiSet(index, rest, value);
    else if (kind == "any") {
        impl_->xinputSet(1, rest, value);
        impl_->joystickSet(1, rest, value);
    }
}

void GameControllers::poll() {
    impl_->dryRun_ = dryRun;
    if (!dryRun) impl_->poll();
    else impl_->ensureDi();
}

void GameControllers::reset() {
    impl_->shutdown();
    for (auto& p : impl_->ppjoy) p = {};
    for (auto& p : impl_->xpad) p = {};
}

void GameControllers::pokeWiimote(int index, int address, int value) {
    impl_->dryRun_ = dryRun;
    auto* w = impl_->wiiAt(index);
    if (!w || w->hid == INVALID_HANDLE_VALUE) return;
    unsigned char b = static_cast<unsigned char>(value);
    impl_->writeMem(w->hid, static_cast<unsigned>(address), &b, 1, w->rumble);
}

void GameControllers::sendWiimote(int index, int report, const std::vector<int>& bytes) {
    impl_->dryRun_ = dryRun;
    auto* w = impl_->wiiAt(index);
    if (!w || w->hid == INVALID_HANDLE_VALUE) return;
    unsigned char buf[22]{};
    buf[0] = static_cast<unsigned char>(report);
    for (size_t i = 0; i < bytes.size() && i + 1 < sizeof(buf); ++i)
        buf[i + 1] = static_cast<unsigned char>(bytes[i]);
    impl_->hidOut(w->hid, buf, static_cast<int>(std::min<size_t>(bytes.size() + 1, sizeof(buf))));
}
