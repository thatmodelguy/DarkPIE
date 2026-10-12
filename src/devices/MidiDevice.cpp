#include "MidiDevice.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

int suffixIndex(const std::string& s, const char* prefix, int fallback = 1) {
    size_t n = std::strlen(prefix);
    if (!startsWith(s, prefix) || s.size() == n) return fallback;
    if (!std::isdigit(static_cast<unsigned char>(s[n]))) return fallback;
    return std::max(0, std::stoi(s.substr(n)));
}

int toMidi(double unit01, int bits) {
    int maxv = (1 << bits) - 1;
    return std::clamp(static_cast<int>(unit01 * maxv + 0.5), 0, maxv);
}

struct NoteName {
    int note = -1;
    std::string extra; // "", "velocity", "aftertouch"
};

int basePitch(const std::string& n) {
    if (n == "c") return 0;
    if (n == "csharp" || n == "db" || n == "dflat") return 1;
    if (n == "d") return 2;
    if (n == "dsharp" || n == "eb" || n == "eflat") return 3;
    if (n == "e") return 4;
    if (n == "f") return 5;
    if (n == "fsharp" || n == "gb" || n == "gflat") return 6;
    if (n == "g") return 7;
    if (n == "gsharp" || n == "ab" || n == "aflat") return 8;
    if (n == "a") return 9;
    if (n == "asharp" || n == "bb" || n == "bflat") return 10;
    if (n == "b") return 11;
    return -1;
}

NoteName parseNote(std::string p) {
    NoteName out;
    if (startsWith(p, "middle")) {
        std::string rest = p.substr(6);
        if (rest.size() >= 8 && rest.substr(rest.size() - 8) == "velocity") {
            out.extra = "velocity";
            rest = rest.substr(0, rest.size() - 8);
        } else if (rest.size() >= 10 && rest.substr(rest.size() - 10) == "aftertouch") {
            out.extra = "aftertouch";
            rest = rest.substr(0, rest.size() - 10);
        }
        int bp = basePitch(rest);
        if (bp >= 0) out.note = 60 + bp;
        return out;
    }
    if (p.size() >= 8 && p.substr(p.size() - 8) == "velocity") {
        out.extra = "velocity";
        p = p.substr(0, p.size() - 8);
    } else if (p.size() >= 10 && p.substr(p.size() - 10) == "aftertouch") {
        out.extra = "aftertouch";
        p = p.substr(0, p.size() - 10);
    }
    size_t i = p.size();
    while (i > 0 && std::isdigit(static_cast<unsigned char>(p[i - 1]))) --i;
    if (i == p.size() || i == 0) return out;
    int oct = std::stoi(p.substr(i));
    int bp = basePitch(p.substr(0, i));
    if (bp < 0 || oct < 0 || oct > 10) return out;
    out.note = oct * 12 + bp;
    if (out.note > 127) out.note = -1;
    return out;
}

int percussionNote(const std::string& p) {
    static const std::unordered_map<std::string, int> names = {
        {"acousticbassdrum", 35}, {"bassdrum2", 35}, {"bassdrum1", 36}, {"bassdrum", 36},
        {"sidestick", 37}, {"acousticsnare", 38}, {"handclap", 39}, {"electricsnare", 40},
        {"lowfloortom", 41}, {"closedhihat", 42}, {"highfloortom", 43}, {"pedalhihat", 44},
        {"lowtom", 45}, {"openhihat", 46}, {"lowmidtom", 47}, {"highmidtom", 48},
        {"crashcymbal1", 49}, {"hightom", 50}, {"ridecymbal1", 51}, {"chinesecymbal", 52},
        {"ridebell", 53}, {"tambourine", 54}, {"splashcymbal", 55}, {"cowbell", 56},
        {"crashcymbal2", 57}, {"vibraslap", 58}, {"ridecymbal2", 59}, {"hibongo", 60},
        {"lowbongo", 61}, {"mutehiconga", 62}, {"openhiconga", 63}, {"lowconga", 64},
        {"hightimbale", 65}, {"lowtimbale", 66}, {"highagogo", 67}, {"lowagogo", 68},
        {"cabasa", 69}, {"maracas", 70}, {"shortwhistle", 71}, {"longwhistle", 72},
        {"shortguiro", 73}, {"longguiro", 74}, {"claves", 75}, {"hiwoodblock", 76},
        {"lowwoodblock", 77}, {"mutecuica", 78}, {"opencuica", 79}, {"mutetriangle", 80},
        {"opentriangle", 81}};
    auto it = names.find(p);
    if (it != names.end()) return it->second;
    if (startsWith(p, "percussion")) return std::clamp(suffixIndex(p, "percussion", 0), 0, 127);
    return -1;
}

int namedControl(const std::string& p) {
    if (p == "bankselect" || p == "bank") return 0;
    if (p == "modulation") return 1;
    if (p == "breath") return 2;
    if (p == "foot") return 4;
    if (p == "portamentotime") return 5;
    if (p == "dataentry") return 6;
    if (p == "volume") return 7;
    if (p == "balance") return 8;
    if (p == "pan") return 10;
    if (p == "expression") return 11;
    if (p == "sustain" || p == "hold") return 64;
    if (p == "portamento") return 65;
    if (p == "sostenuto") return 66;
    if (p == "soft") return 67;
    return -1;
}
}

struct MidiDevice::Impl {
    bool dryRun = false;
    int deviceOut = 1;
    int deviceIn = 1;
    int defaultChannel = 1;
    bool generalMidi = true;
    HMIDIOUT out = nullptr;
    HMIDIIN in = nullptr;
    int openedOut = -2;
    int openedIn = -2;

    struct Chan {
        bool noteOn[128]{};
        double velocity[128]{};
        double aftertouch[128]{};
        double control[128]{};
        int instrument = 1;
        double pitchWheel = 0.5;
        double channelPressure = 0;
        int numberedNote[4]{-1, -1, -1, -1};
    };
    Chan outCh[16];
    Chan inCh[16];
    std::mutex inMu;

    ~Impl() { close(); }

    static void CALLBACK inProc(HMIDIIN, UINT msg, DWORD_PTR inst, DWORD_PTR dw1, DWORD_PTR) {
        if (msg != MIM_DATA) return;
        auto* self = reinterpret_cast<Impl*>(inst);
        self->onShort(static_cast<DWORD>(dw1));
    }

    void onShort(DWORD pack) {
        unsigned st = pack & 0xFF;
        unsigned d1 = (pack >> 8) & 0xFF;
        unsigned d2 = (pack >> 16) & 0xFF;
        int ch = st & 0x0F;
        unsigned cmd = st & 0xF0;
        std::lock_guard<std::mutex> lock(inMu);
        Chan& c = inCh[ch];
        if (cmd == 0x90) {
            if (d2 == 0) {
                c.noteOn[d1] = false;
                c.velocity[d1] = 0;
            } else {
                c.noteOn[d1] = true;
                c.velocity[d1] = d2 / 127.0;
            }
        } else if (cmd == 0x80) {
            c.noteOn[d1] = false;
            c.velocity[d1] = 0;
        } else if (cmd == 0xA0) {
            c.aftertouch[d1] = d2 / 127.0;
        } else if (cmd == 0xB0) {
            c.control[d1] = d2 / 127.0;
        } else if (cmd == 0xC0) {
            c.instrument = static_cast<int>(d1) + 1;
        } else if (cmd == 0xD0) {
            c.channelPressure = d1 / 127.0;
        } else if (cmd == 0xE0) {
            int v = static_cast<int>(d1 | (d2 << 7));
            c.pitchWheel = v / 16383.0;
        }
    }

    void close() {
        if (in) {
            midiInStop(in);
            midiInClose(in);
            in = nullptr;
        }
        if (out) {
            midiOutClose(out);
            out = nullptr;
        }
        openedOut = openedIn = -2;
    }

    UINT winOutId(int pieDev) const {
        if (pieDev <= 0) return MIDI_MAPPER;
        UINT n = midiOutGetNumDevs();
        if (n == 0) return MIDI_MAPPER;
        return static_cast<UINT>(std::clamp(pieDev - 1, 0, static_cast<int>(n) - 1));
    }

    UINT winInId(int pieDev) const {
        UINT n = midiInGetNumDevs();
        if (n == 0) return 0;
        return static_cast<UINT>(std::clamp(std::max(pieDev, 1) - 1, 0, static_cast<int>(n) - 1));
    }

    void ensureOut(int pieDev) {
        if (dryRun) return;
        if (out && openedOut == pieDev) return;
        if (out) {
            midiOutClose(out);
            out = nullptr;
        }
        openedOut = pieDev;
        midiOutOpen(&out, winOutId(pieDev), 0, 0, CALLBACK_NULL);
    }

    void ensureIn(int pieDev) {
        if (dryRun) return;
        if (in && openedIn == pieDev) return;
        if (in) {
            midiInStop(in);
            midiInClose(in);
            in = nullptr;
        }
        openedIn = pieDev;
        if (midiInGetNumDevs() == 0) return;
        if (midiInOpen(&in, winInId(pieDev), reinterpret_cast<DWORD_PTR>(&inProc), reinterpret_cast<DWORD_PTR>(this),
                       CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(in);
        } else {
            in = nullptr;
        }
    }

    void sendShort(int pieDev, unsigned status, unsigned d1, unsigned d2) {
        ensureOut(pieDev);
        if (dryRun || !out) return;
        midiOutShortMsg(out, status | (d1 << 8) | (d2 << 16));
    }

    void noteOn(int pieDev, int ch, int note, double vel) {
        ch = std::clamp(ch, 1, 16);
        note = std::clamp(note, 0, 127);
        Chan& c = outCh[ch - 1];
        int v = toMidi(vel <= 0 ? 0 : (vel > 1 ? 1 : vel), 7);
        if (v <= 0) {
            c.noteOn[note] = false;
            c.velocity[note] = 0;
            sendShort(pieDev, 0x80 | (ch - 1), note, 64);
        } else {
            c.noteOn[note] = true;
            c.velocity[note] = vel;
            sendShort(pieDev, 0x90 | (ch - 1), note, v);
        }
    }

    std::string outName(int pieDev) const {
        MIDIOUTCAPS caps{};
        UINT id = winOutId(pieDev);
        if (id == MIDI_MAPPER) return "MIDI Mapper";
        if (midiOutGetDevCaps(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR) return "";
        char a[MAXPNAMELEN]{};
        WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, a, MAXPNAMELEN, nullptr, nullptr);
        return a;
    }

    std::string inName(int pieDev) const {
        MIDIINCAPS caps{};
        if (midiInGetNumDevs() == 0) return "";
        if (midiInGetDevCaps(winInId(pieDev), &caps, sizeof(caps)) != MMSYSERR_NOERROR) return "";
        char a[MAXPNAMELEN]{};
        WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, a, MAXPNAMELEN, nullptr, nullptr);
        return a;
    }

    int findOutByName(const std::string& needle, int nth) {
        int seen = 0;
        UINT n = midiOutGetNumDevs();
        for (UINT i = 0; i < n; ++i) {
            MIDIOUTCAPS caps{};
            if (midiOutGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
            char a[MAXPNAMELEN]{};
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, a, MAXPNAMELEN, nullptr, nullptr);
            if (lower(a).find(needle) == std::string::npos) continue;
            ++seen;
            if (seen == nth) return static_cast<int>(i) + 1;
        }
        return -1;
    }

    struct Path {
        int outDev = -1;
        int inDev = -1;
        int channel = -1;
        std::string prop;
    };

    Path parsePath(const std::string& root, const std::string& restRaw) {
        Path p;
        std::string r = lower(root);
        std::string rest = lower(restRaw);
        rest.erase(std::remove(rest.begin(), rest.end(), '_'), rest.end());
        if (startsWith(r, "midi") && r.size() > 4 && std::isdigit(static_cast<unsigned char>(r[4]))) {
            p.outDev = suffixIndex(r, "midi", 1);
            p.inDev = p.outDev;
        }
        std::string acc;
        size_t i = 0;
        auto nextPart = [&]() {
            size_t d = rest.find('.', i);
            std::string part = d == std::string::npos ? rest.substr(i) : rest.substr(i, d - i);
            i = d == std::string::npos ? rest.size() : d + 1;
            return part;
        };
        while (i < rest.size()) {
            std::string part = nextPart();
            if (startsWith(part, "channel") && part.size() > 7 && std::isdigit(static_cast<unsigned char>(part[7]))) {
                p.channel = suffixIndex(part, "channel", 1);
                continue;
            }
            if (part == "mapper") {
                p.outDev = 0;
                continue;
            }
            if (startsWith(part, "yoke") || startsWith(part, "soundcard") || startsWith(part, "software") ||
                startsWith(part, "wavetable") || startsWith(part, "playback") || startsWith(part, "external")) {
                std::string key = part;
                while (!key.empty() && std::isdigit(static_cast<unsigned char>(key.back()))) key.pop_back();
                int nth = 1;
                if (part.size() > key.size()) nth = std::max(1, std::stoi(part.substr(key.size())));
                int found = findOutByName(key, nth);
                if (found >= 0) p.outDev = found;
                continue;
            }
            p.prop = part;
            if (i < rest.size()) p.prop += "." + rest.substr(i);
            break;
        }
        return p;
    }

    int chanOf(const Path& p) const { return p.channel > 0 ? p.channel : defaultChannel; }
    int outOf(const Path& p) const { return p.outDev >= 0 ? p.outDev : deviceOut; }
    int inOf(const Path& p) const { return p.inDev >= 0 ? p.inDev : deviceIn; }

    bool anyInNote(int note) {
        std::lock_guard<std::mutex> lock(inMu);
        for (auto& c : inCh)
            if (c.noteOn[note]) return true;
        return false;
    }

    double anyInVel(int note) {
        std::lock_guard<std::mutex> lock(inMu);
        double v = 0;
        for (auto& c : inCh)
            if (c.velocity[note] > v) v = c.velocity[note];
        return v;
    }

    int firstInNote() {
        std::lock_guard<std::mutex> lock(inMu);
        for (int n = 0; n < 128; ++n)
            for (auto& c : inCh)
                if (c.noteOn[n]) return n;
        return -1;
    }

    Value get(const Path& p) {
        ensureIn(inOf(p));
        const std::string& prop = p.prop;
        int ch = std::clamp(chanOf(p), 1, 16);
        Chan& oc = outCh[ch - 1];
        if (prop == "deviceout") return Value::number(outOf(p));
        if (prop == "devicein") return Value::number(inOf(p));
        if (prop == "outdevname") return Value::string(outName(outOf(p)));
        if (prop == "indevname") return Value::string(inName(inOf(p)));
        if (prop == "outversion" || prop == "inversion" || prop == "outtech" || prop == "intech")
            return Value::number(0);
        if (prop == "defaultchannel") return Value::number(defaultChannel);
        if (prop == "generalmidi") return Value::boolean(generalMidi);
        if (prop == "instrument") return Value::number(oc.instrument);
        if (prop == "instrument0") return Value::number(oc.instrument - 1);
        if (prop == "pitchwheel") return Value::number(oc.pitchWheel);
        if (prop == "channelpressure") return Value::number(oc.channelPressure);
        if (prop == "firstnote") return Value::number(firstInNote());
        if (prop == "secondnote" || prop == "thirdnote" || prop == "fourthnote") return Value::number(-1);
        if (prop == "count" || prop == "outdevicecount") return Value::number(midiOutGetNumDevs());
        if (prop == "indevicecount") return Value::number(midiInGetNumDevs());

        int cc = namedControl(prop);
        int coarse = -1, fine = -1;
        if (startsWith(prop, "control") && prop.size() > 7) {
            std::string t = prop.substr(7);
            bool isFine = false, isCoarse = false, isByte = false;
            (void)isByte;
            if (startsWith(prop, "bytecontrol")) {
                t = prop.substr(11);
                isByte = true;
            }
            if (t.size() >= 4 && t.substr(t.size() - 4) == "fine") {
                isFine = true;
                t = t.substr(0, t.size() - 4);
            } else if (t.size() >= 6 && t.substr(t.size() - 6) == "coarse") {
                isCoarse = true;
                t = t.substr(0, t.size() - 6);
            }
            if (!t.empty() && std::isdigit(static_cast<unsigned char>(t[0]))) {
                int n = std::stoi(t);
                if (isFine) fine = n;
                else if (isCoarse) coarse = n;
                else cc = n;
            }
        }
        if (startsWith(prop, "bytecontrol") && prop.size() > 11 && std::isdigit(static_cast<unsigned char>(prop[11]))) {
            cc = suffixIndex(prop, "bytecontrol", 0);
        }
        if (cc >= 0) return Value::number(oc.control[std::clamp(cc, 0, 127)]);
        if (coarse >= 0) return Value::number(oc.control[std::clamp(coarse, 0, 127)]);
        if (fine >= 0) return Value::number(oc.control[std::clamp(fine, 0, 127)]);

        NoteName nn = parseNote(prop);
        if (nn.note >= 0) {
            if (nn.extra == "velocity") return Value::number(anyInVel(nn.note));
            if (nn.extra == "aftertouch") {
                std::lock_guard<std::mutex> lock(inMu);
                double a = 0;
                for (auto& c : inCh)
                    if (c.aftertouch[nn.note] > a) a = c.aftertouch[nn.note];
                return Value::number(a);
            }
            return Value::boolean(anyInNote(nn.note));
        }
        int perc = percussionNote(prop);
        if (perc >= 0) return Value::boolean(anyInNote(perc));
        return Value::boolean(false);
    }

    void setCC(int pieDev, int ch, int cc, double unit01, bool fourteen) {
        ch = std::clamp(ch, 1, 16);
        cc = std::clamp(cc, 0, 127);
        outCh[ch - 1].control[cc] = unit01;
        if (fourteen && cc <= 31) {
            int v = toMidi(unit01, 14);
            sendShort(pieDev, 0xB0 | (ch - 1), cc, (v >> 7) & 0x7F);
            sendShort(pieDev, 0xB0 | (ch - 1), cc + 32, v & 0x7F);
            outCh[ch - 1].control[cc + 32] = (v & 0x7F) / 127.0;
        } else {
            sendShort(pieDev, 0xB0 | (ch - 1), cc, toMidi(unit01, 7));
        }
    }

    void set(const Path& p, const Value& v) {
        const std::string& prop = p.prop;
        int ch = std::clamp(chanOf(p), 1, 16);
        int pieDev = outOf(p);
        Chan& oc = outCh[ch - 1];
        if (prop == "deviceout") {
            deviceOut = static_cast<int>(v.asNumber());
            return;
        }
        if (prop == "devicein") {
            deviceIn = static_cast<int>(v.asNumber());
            return;
        }
        if (prop == "defaultchannel") {
            defaultChannel = std::clamp(static_cast<int>(v.asNumber()), 1, 16);
            return;
        }
        if (prop == "generalmidi") {
            generalMidi = v.isTruthy();
            return;
        }
        if (prop == "pitchbendrange") {
            setCC(pieDev, ch, 101, 0, false);
            setCC(pieDev, ch, 100, 0, false);
            setCC(pieDev, ch, 6, std::clamp(v.asNumber() / 24.0, 0.0, 1.0), true);
            return;
        }
        if (prop == "mastervolume") {
            setCC(pieDev, ch, 7, v.asNumber(), true);
            return;
        }
        if (prop == "setupfile") {
            if (dryRun) return;
            ensureOut(pieDev);
            if (!out) return;
            FILE* f = nullptr;
            fopen_s(&f, v.asString().c_str(), "rb");
            if (!f) return;
            std::vector<unsigned char> syx(4096);
            size_t n = fread(syx.data(), 1, syx.size(), f);
            fclose(f);
            syx.resize(n);
            MIDIHDR hdr{};
            hdr.lpData = reinterpret_cast<LPSTR>(syx.data());
            hdr.dwBufferLength = static_cast<DWORD>(syx.size());
            midiOutPrepareHeader(out, &hdr, sizeof(hdr));
            midiOutLongMsg(out, &hdr, sizeof(hdr));
            midiOutUnprepareHeader(out, &hdr, sizeof(hdr));
            return;
        }
        if (prop == "instrument") {
            oc.instrument = std::clamp(static_cast<int>(v.asNumber()), 1, 128);
            sendShort(pieDev, 0xC0 | (ch - 1), oc.instrument - 1, 0);
            return;
        }
        if (prop == "instrument0") {
            oc.instrument = std::clamp(static_cast<int>(v.asNumber()), 0, 127) + 1;
            sendShort(pieDev, 0xC0 | (ch - 1), oc.instrument - 1, 0);
            return;
        }
        if (prop == "pitchwheel") {
            oc.pitchWheel = std::clamp(v.asNumber(), 0.0, 1.0);
            int n = toMidi(oc.pitchWheel, 14);
            sendShort(pieDev, 0xE0 | (ch - 1), n & 0x7F, (n >> 7) & 0x7F);
            return;
        }
        if (prop == "channelpressure") {
            oc.channelPressure = std::clamp(v.asNumber(), 0.0, 1.0);
            sendShort(pieDev, 0xD0 | (ch - 1), toMidi(oc.channelPressure, 7), 0);
            return;
        }
        if (prop == "firstnote" || prop == "secondnote" || prop == "thirdnote" || prop == "fourthnote") {
            int slot = prop[0] == 'f' ? 0 : prop[0] == 's' ? 1 : prop[0] == 't' ? 2 : 3;
            int prev = oc.numberedNote[slot];
            int n = static_cast<int>(v.asNumber());
            if (prev >= 0) noteOn(pieDev, ch, prev, 0);
            oc.numberedNote[slot] = n;
            if (n >= 0) noteOn(pieDev, ch, n, 0.5);
            return;
        }
        int cc = namedControl(prop);
        if (startsWith(prop, "control") && prop.size() > 7 && std::isdigit(static_cast<unsigned char>(prop[7]))) {
            std::string t = prop.substr(7);
            bool fine = false, coarse = false;
            if (t.size() >= 4 && t.substr(t.size() - 4) == "fine") {
                fine = true;
                t = t.substr(0, t.size() - 4);
            } else if (t.size() >= 6 && t.substr(t.size() - 6) == "coarse") {
                coarse = true;
                t = t.substr(0, t.size() - 6);
            }
            int n = std::stoi(t);
            if (fine) setCC(pieDev, ch, n <= 31 ? n + 32 : n, v.asNumber(), false);
            else if (coarse) setCC(pieDev, ch, n, v.asNumber(), false);
            else setCC(pieDev, ch, n, v.asNumber(), n <= 31);
            return;
        }
        if (startsWith(prop, "bytecontrol")) {
            setCC(pieDev, ch, suffixIndex(prop, "bytecontrol", 0), v.asNumber(), false);
            return;
        }
        if (cc >= 0) {
            setCC(pieDev, ch, cc, v.asNumber(), cc <= 31);
            return;
        }

        int perc = percussionNote(prop);
        if (perc >= 0) {
            noteOn(pieDev, 10, perc, v.isTruthy() ? (v.kind == ValueKind::Number ? v.asNumber() : 0.5) : 0);
            return;
        }
        NoteName nn = parseNote(prop);
        if (nn.note >= 0) {
            if (nn.extra == "velocity") {
                noteOn(pieDev, ch, nn.note, v.asNumber());
                return;
            }
            if (nn.extra == "aftertouch") {
                oc.aftertouch[nn.note] = v.asNumber();
                sendShort(pieDev, 0xA0 | (ch - 1), nn.note, toMidi(v.asNumber(), 7));
                return;
            }
            double vel = v.kind == ValueKind::Number ? v.asNumber() : (v.isTruthy() ? 0.5 : 0);
            noteOn(pieDev, ch, nn.note, vel);
        }
    }
};

MidiDevice::MidiDevice() : impl_(new Impl) {}
MidiDevice::~MidiDevice() { delete impl_; }

bool MidiDevice::handles(const std::string& root) const {
    std::string r = lower(root);
    return r == "midi" || (startsWith(r, "midi") && r.size() > 4 && std::isdigit(static_cast<unsigned char>(r[4])));
}

Value MidiDevice::get(const std::string& root, const std::string& rest) {
    impl_->dryRun = dryRun;
    return impl_->get(impl_->parsePath(root, rest));
}

void MidiDevice::set(const std::string& root, const std::string& rest, const Value& value) {
    impl_->dryRun = dryRun;
    impl_->set(impl_->parsePath(root, rest), value);
}

void MidiDevice::reset() {
    bool wasDry = impl_->dryRun;
    impl_->close();
    impl_->deviceOut = 1;
    impl_->deviceIn = 1;
    impl_->defaultChannel = 1;
    impl_->generalMidi = true;
    impl_->dryRun = wasDry;
    for (auto& c : impl_->outCh) c = {};
    std::lock_guard<std::mutex> lock(impl_->inMu);
    for (auto& c : impl_->inCh) c = {};
}
