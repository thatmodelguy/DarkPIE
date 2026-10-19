#include "SpeechDevice.h"

#include <Windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <sapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <sstream>

namespace {
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

std::string wideToUtf8(const wchar_t* s) {
    if (!s) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

ISpObjectToken* tokenFromId(const std::wstring& tokenId) {
    if (tokenId.empty()) return nullptr;
    ISpObjectToken* token = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_ISpObjectToken,
                                  reinterpret_cast<void**>(&token));
    if (FAILED(hr) || !token) return nullptr;
    hr = token->SetId(nullptr, tokenId.c_str(), FALSE);
    if (FAILED(hr)) {
        token->Release();
        return nullptr;
    }
    return token;
}

ISpObjectToken* defaultToken(const WCHAR* categoryId) {
    ISpObjectTokenCategory* category = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                                  reinterpret_cast<void**>(&category));
    if (FAILED(hr) || !category) return nullptr;
    hr = category->SetId(categoryId, FALSE);
    WCHAR* tokenId = nullptr;
    if (SUCCEEDED(hr)) hr = category->GetDefaultTokenId(&tokenId);
    category->Release();
    if (FAILED(hr) || !tokenId) return nullptr;

    ISpObjectToken* token = nullptr;
    hr = CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_ISpObjectToken,
                          reinterpret_cast<void**>(&token));
    if (SUCCEEDED(hr) && token) hr = token->SetId(nullptr, tokenId, FALSE);
    CoTaskMemFree(tokenId);
    if (FAILED(hr) || !token) {
        if (token) token->Release();
        return nullptr;
    }
    return token;
}

const char* digitWord(char d) {
    switch (d) {
    case '0': return "zero";
    case '1': return "one";
    case '2': return "two";
    case '3': return "three";
    case '4': return "four";
    case '5': return "five";
    case '6': return "six";
    case '7': return "seven";
    case '8': return "eight";
    case '9': return "nine";
    default: return nullptr;
    }
}

std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string foldPhrase(std::string s) {
    auto words = splitWords(s);
    std::string out;
    for (auto& w : words) {
        if (w == "backwards") w = "backward";
        else if (w == "forwards" || w == "foward") w = "forward";
        if (!out.empty()) out.push_back(' ');
        out += w;
    }
    return out;
}

bool phraseContains(const std::string& heard, const std::string& key) {
    std::string a = foldPhrase(heard);
    std::string b = foldPhrase(key);
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    auto aw = splitWords(a);
    auto bw = splitWords(b);
    if (bw.size() > aw.size()) return false;
    for (size_t i = 0; i + bw.size() <= aw.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < bw.size(); ++j) {
            if (aw[i + j] != bw[j]) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

std::string spokenCanonical(std::string s) {
    if (s == "zero" || s == "oh" || s == "nought") return "0";
    if (s == "one") return "1";
    if (s == "two" || s == "to" || s == "too") return "2";
    if (s == "three") return "3";
    if (s == "four" || s == "for") return "4";
    if (s == "five") return "5";
    if (s == "six") return "6";
    if (s == "seven") return "7";
    if (s == "eight" || s == "ate") return "8";
    if (s == "nine") return "9";
    return s;
}

std::vector<std::string> grammarForms(const std::string& phrase) {
    std::vector<std::string> forms;
    auto add = [&](std::string f) {
        if (f.empty()) return;
        for (char& c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const auto& e : forms)
            if (e == f) return;
        forms.push_back(f);
    };
    add(phrase);
    std::string n = foldPhrase(forms.empty() ? phrase : forms.front());
    add(n);
    if (n.find("backward") != std::string::npos && n.find("backwards") == std::string::npos) {
        std::string x = n;
        auto pos = x.find("backward");
        x.replace(pos, 8, "backwards");
        add(x);
    }
    if (n == "walk") add("walk forward");
    if (n.find("foward") != std::string::npos) {
        std::string x = n;
        auto pos = x.find("foward");
        x.replace(pos, 6, "forward");
        add(x);
    }
    if (phrase.size() == 1 && phrase[0] >= '0' && phrase[0] <= '9') {
        if (const char* w = digitWord(phrase[0])) add(w);
    }
    return forms;
}

std::vector<std::string> canonWords(const std::string& phrase) {
    std::string folded = foldPhrase(phrase);
    auto words = splitWords(folded);
    for (auto& w : words) w = spokenCanonical(w);
    return words;
}

std::wstring lowerWide(std::wstring s) {
    for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

std::wstring waveKey(const std::wstring& tokenOrEndpoint) {
    auto p = tokenOrEndpoint.rfind(L"{0.0.1.");
    if (p == std::wstring::npos) {
        p = tokenOrEndpoint.rfind(L'{');
        if (p == std::wstring::npos) return {};
    }
    return lowerWide(tokenOrEndpoint.substr(p));
}

struct VolumeCom {
    bool owned = false;
    VolumeCom() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        owned = (hr == S_OK);
    }
    ~VolumeCom() {
        if (owned) CoUninitialize();
    }
};

IMMDevice* openCaptureDevice(IMMDeviceEnumerator* en, const std::wstring& wave) {
    if (!en) return nullptr;
    if (wave.empty()) {
        IMMDevice* dev = nullptr;
        if (FAILED(en->GetDefaultAudioEndpoint(eCapture, eConsole, &dev))) return nullptr;
        return dev;
    }
    IMMDeviceCollection* col = nullptr;
    if (FAILED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &col)) || !col) return nullptr;
    UINT count = 0;
    col->GetCount(&count);
    IMMDevice* found = nullptr;
    for (UINT i = 0; i < count && !found; ++i) {
        IMMDevice* dev = nullptr;
        if (FAILED(col->Item(i, &dev)) || !dev) continue;
        LPWSTR id = nullptr;
        if (SUCCEEDED(dev->GetId(&id)) && id) {
            if (lowerWide(id).find(wave) != std::wstring::npos) found = dev;
            CoTaskMemFree(id);
        }
        if (found != dev) dev->Release();
    }
    col->Release();
    return found;
}

bool setDeviceScalar(IMMDevice* dev, float scalar, float* previous) {
    if (!dev) return false;
    IAudioEndpointVolume* vol = nullptr;
    HRESULT hr = dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&vol));
    if (FAILED(hr) || !vol) return false;
    float current = 1.f;
    if (FAILED(vol->GetMasterVolumeLevelScalar(&current))) current = 1.f;
    if (previous) *previous = current;
    scalar = std::clamp(scalar, 0.f, 1.f);
    // Writing the same level still restarts the capture graph on some devices.
    if (std::fabs(current - scalar) < 0.004f) {
        vol->Release();
        return true;
    }
    hr = vol->SetMasterVolumeLevelScalar(scalar, nullptr);
    vol->Release();
    return SUCCEEDED(hr);
}

bool adjustCapture(const std::wstring& tokenOrEndpoint, float scalar, float* previous, std::string* endpointOut) {
    VolumeCom com;
    IMMDeviceEnumerator* en = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&en));
    if (FAILED(hr) || !en) return false;
    IMMDevice* dev = openCaptureDevice(en, waveKey(tokenOrEndpoint));
    bool ok = false;
    if (dev) {
        if (endpointOut) {
            LPWSTR id = nullptr;
            if (SUCCEEDED(dev->GetId(&id)) && id) {
                *endpointOut = wideToUtf8(id);
                CoTaskMemFree(id);
            }
        }
        ok = setDeviceScalar(dev, scalar, previous);
        dev->Release();
    }
    en->Release();
    return ok;
}

// SAPI's capture buffer defaults to 500 ms, and 200 ms is the smallest capacity
// it will accept. The notify size is the piece of audio the engine actually
// reads, so a guess can start after about 20 ms instead of waiting out the buffer.
void tightenCaptureBuffer(ISpRecognizer* recognizer) {
    if (!recognizer) return;
    ISpStreamFormat* stream = nullptr;
    if (FAILED(recognizer->GetInputStream(&stream)) || !stream) return;
    ISpAudio* audio = nullptr;
    if (SUCCEEDED(stream->QueryInterface(IID_ISpAudio, reinterpret_cast<void**>(&audio))) && audio) {
        SPAUDIOBUFFERINFO info{};
        info.ulMsMinNotification = 20;
        info.ulMsBufferSize = 200;
        info.ulMsEventBias = 0;
        audio->SetBufferInfo(&info);

        ULONG notify = 640;
        GUID formatId{};
        WAVEFORMATEX* format = nullptr;
        if (SUCCEEDED(stream->GetFormat(&formatId, &format)) && format && format->nAvgBytesPerSec > 0) {
            notify = format->nAvgBytesPerSec / 50;
            if (format->nBlockAlign > 1) notify -= notify % format->nBlockAlign;
            if (notify < format->nBlockAlign) notify = format->nBlockAlign;
            CoTaskMemFree(format);
        }
        audio->SetBufferNotifySize(notify);
        audio->Release();
    }
    stream->Release();
}
}

SpeechDevice::SpeechDevice() = default;

SpeechDevice::~SpeechDevice() { stop(); }

void SpeechDevice::setMicrophoneId(std::string id) { microphoneId_ = std::move(id); }

std::vector<MicrophoneInfo> SpeechDevice::listMicrophones() {
    std::vector<MicrophoneInfo> out;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = (hr == S_OK);
    ISpObjectTokenCategory* category = nullptr;
    hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                          reinterpret_cast<void**>(&category));
    if (FAILED(hr) || !category) {
        if (uninit) CoUninitialize();
        return out;
    }
    hr = category->SetId(SPCAT_AUDIOIN, FALSE);
    IEnumSpObjectTokens* enumer = nullptr;
    if (SUCCEEDED(hr)) hr = category->EnumTokens(nullptr, nullptr, &enumer);
    category->Release();
    if (FAILED(hr) || !enumer) {
        if (uninit) CoUninitialize();
        return out;
    }
    ISpObjectToken* token = nullptr;
    ULONG fetched = 0;
    while (enumer->Next(1, &token, &fetched) == S_OK && fetched == 1 && token) {
        MicrophoneInfo info;
        WCHAR* id = nullptr;
        if (SUCCEEDED(token->GetId(&id)) && id) {
            info.id = wideToUtf8(id);
            CoTaskMemFree(id);
        }
        WCHAR* name = nullptr;
        if (SUCCEEDED(token->GetStringValue(nullptr, &name)) && name) {
            info.name = wideToUtf8(name);
            CoTaskMemFree(name);
        }
        if (info.name.empty()) info.name = info.id;
        if (!info.id.empty()) out.push_back(std::move(info));
        token->Release();
        token = nullptr;
    }
    enumer->Release();
    if (uninit) CoUninitialize();
    return out;
}

std::string SpeechDevice::normalize(std::string s) const {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string out;
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!out.empty()) space = true;
        } else {
            if (space) out.push_back(' ');
            space = false;
            out.push_back(c);
        }
    }
    return out;
}

bool SpeechDevice::confidenceOk(int required, int sapiConfidence, bool partial) const {
    if (required < 0) required = 2;
    if (required > 6) required = 6;
    bool needComplete = required >= 4;
    if (needComplete && partial) return false;
    int band = required;
    if (required >= 4) band = required - 4;
    if (band <= 0) return true;
    if (band == 1) return sapiConfidence >= SP_LOW_CONFIDENCE;
    if (band == 2) return sapiConfidence >= SP_NORMAL_CONFIDENCE;
    return sapiConfidence >= SP_HIGH_CONFIDENCE;
}

bool SpeechDevice::start(const std::vector<std::string>& phrases, std::string& error) {
    stop();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) comInit_ = true;
    else if (hr != RPC_E_CHANGED_MODE && FAILED(hr)) {
        error = "COM initialization failed for SAPI.";
        status_ = error;
        return false;
    }

    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void**>(&voice_));
    if (FAILED(hr)) {
        error = "Could not create SAPI voice (ISpVoice).";
        status_ = error;
        return false;
    }

    // In-process recognizer: does not launch or share Windows Speech Recognition.
    hr = CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL, IID_ISpRecognizer,
                          reinterpret_cast<void**>(&recognizer_));
    if (FAILED(hr) || !recognizer_) {
        error = "Could not create in-process SAPI recognizer.";
        status_ = error;
        return false;
    }

    if (ISpObjectToken* recoToken = defaultToken(SPCAT_RECOGNIZERS)) {
        hr = recognizer_->SetRecognizer(recoToken);
        recoToken->Release();
        if (FAILED(hr)) {
            error = "Could not select the desktop speech engine.";
            status_ = error;
            return false;
        }
    }

    ISpObjectToken* audioToken = nullptr;
    if (!microphoneId_.empty()) audioToken = tokenFromId(utf8ToWide(microphoneId_));
    if (!audioToken) audioToken = defaultToken(SPCAT_AUDIOIN);
    if (audioToken) {
        hr = recognizer_->SetInput(audioToken, TRUE);
        audioToken->Release();
        if (FAILED(hr)) {
            error = "Could not open the selected microphone.";
            status_ = error;
            return false;
        }
    } else {
        error = "No microphone found for speech input.";
        status_ = error;
        return false;
    }

    hr = recognizer_->CreateRecoContext(&context_);
    if (FAILED(hr) || !context_) {
        error = "Could not create SAPI recognition context.";
        status_ = error;
        return false;
    }

    // Win32 event, not a window message, so a guess can wake the script immediately.
    context_->SetNotifyWin32Event();
    // One alternate. Extra alternates make the engine finish scoring before it will guess.
    context_->SetMaxAlternates(1);
    recognizer_->SetPropertyNum(L"CFGConfidenceRejectionThreshold", 0);
    // Trailing silence after a finished word. This engine will not keep a
    // value below 50 ms, or below 100 ms when the word could still be extended.
    // A long word finishes during the word, so it never shows this pause.
    // Set once, before the engine is running.
    recognizer_->SetPropertyNum(L"ComplexResponseSpeed", 10000);
    recognizer_->SetPropertyNum(L"ResponseSpeed", 50);
    recognizer_->SetPropertyNum(L"ComplexResponseSpeed", 100);
    tightenCaptureBuffer(recognizer_);
    ULONGLONG interest = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS) | SPFEI(SPEI_PHRASE_START) |
                         SPFEI(SPEI_SOUND_START) | SPFEI(SPEI_SOUND_END);
    context_->SetInterest(interest, interest);

    hr = context_->CreateGrammar(1, &grammar_);
    if (FAILED(hr) || !grammar_) {
        error = "Could not create SAPI grammar.";
        status_ = error;
        return false;
    }

    grammar_->ResetGrammar(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    commands_.clear();
    asleep_ = false;
    wentToSleep_ = false;
    wokeUp_ = false;
    std::vector<std::string> listen = phrases;
    if (sleepWakeEnabled_) {
        listen.push_back("go to sleep");
        listen.push_back("wake up");
        listen.push_back("wakeup");
    }
    rebuildCommands(listen);
    // Every command is an alternative of one top-level rule, so each spoken
    // command is its own result. A repeating rule made "use use use" one long
    // phrase that only finished when the speaking stopped.
    SPSTATEHANDLE hItem = nullptr;
    hr = grammar_->GetRule(L"item", 1, SPRAF_TopLevel | SPRAF_Active, TRUE, &hItem);
    ULONG propId = 1;
    auto addPhrase = [&](const std::string& form, float weight) -> HRESULT {
        if (form.empty() || !hItem) return S_OK;
        std::wstring w = utf8ToWide(form);
        SPPROPERTYINFO prop{};
        prop.pszName = L"cmd";
        prop.ulId = propId++;
        prop.pszValue = w.c_str();
        prop.vValue.vt = VT_EMPTY;
        HRESULT r = grammar_->AddWordTransition(hItem, nullptr, w.c_str(), L" ", SPWT_LEXICAL, weight, &prop);
        if (FAILED(r)) r = grammar_->AddWordTransition(hItem, nullptr, w.c_str(), L" ", SPWT_LEXICAL, weight, nullptr);
        return r;
    };
    if (FAILED(hr)) {
        error = "Could not build speech command grammar.";
        status_ = error;
        return false;
    }
    if (listen.empty()) {
        hr = addPhrase("darkpie idle", 1.0f);
    } else {
        for (const auto& p : listen) {
            if (!listensFor(p)) continue;
            std::string n = normalize(p);
            int words = 1;
            for (char c : n)
                if (c == ' ') ++words;
            const float weight = (words <= 1) ? 50.0f : 1.0f;
            for (const auto& form : grammarForms(p)) {
                hr = addPhrase(form, weight);
                if (FAILED(hr)) break;
            }
            if (FAILED(hr)) break;
        }
    }
    if (FAILED(hr)) {
        error = "Could not build speech command grammar.";
        status_ = error;
        return false;
    }
    hr = grammar_->Commit(0);
    if (FAILED(hr)) {
        error = "Could not commit speech command grammar.";
        status_ = error;
        return false;
    }
    grammar_->SetRuleState(nullptr, nullptr, SPRS_ACTIVE);

    context_->SetContextState(SPCS_ENABLED);
    recognizer_->SetRecoState(SPRST_ACTIVE_ALWAYS);
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        audioStartQpc_ = t.QuadPart;
        audioBytesPerSec_ = 0.0;
        GUID fmtId{};
        WAVEFORMATEX* wf = nullptr;
        if (SUCCEEDED(recognizer_->GetFormat(SPWF_SRENGINE, &fmtId, &wf)) && wf) {
            audioBytesPerSec_ = wf->nAvgBytesPerSec;
            CoTaskMemFree(wf);
        }
    }
    firedThisUtterance_.clear();
    pendingPulses_.clear();
    progress_.clear();
    utterancePos_ = 0;
    utteranceClosed_ = false;
    expectNewUtterance_ = true;

    ready_ = true;
    if (listen.empty()) status_ = "Speech ready (no said() phrases)";
    else if (sleepWakeEnabled_)
        status_ = "Listening for " + std::to_string(phrases.size()) +
                  " script commands + sleep/wake";
    else
        status_ = "Listening for " + std::to_string(phrases.size()) + " script commands";
    return true;
}

bool SpeechDevice::forceEndpointVolume(const std::string& endpointId, float scalar) {
    if (endpointId.empty()) return false;
    return adjustCapture(utf8ToWide(endpointId), scalar, nullptr, nullptr);
}

void SpeechDevice::rebuildCommands(const std::vector<std::string>& phrases) {
    commands_.clear();
    for (const auto& p : phrases) {
        std::string display = normalize(p);
        if (display.empty()) continue;
        CommandPhrase cmd;
        cmd.display = display;
        cmd.words = canonWords(display);
        if (cmd.words.empty()) continue;
        bool dup = false;
        for (const auto& existing : commands_) {
            if (existing.words == cmd.words) {
                dup = true;
                break;
            }
        }
        if (!dup) commands_.push_back(std::move(cmd));
    }
    // A command that starts with a whole shorter command can never fire, since
    // the shorter one fires first. Listening for it anyway makes the engine
    // wait after the shorter one in case the longer one follows.
    std::vector<CommandPhrase> kept;
    for (const auto& cmd : commands_) {
        const CommandPhrase* by = nullptr;
        for (const auto& other : commands_) {
            if (other.words.size() >= cmd.words.size()) continue;
            if (std::equal(other.words.begin(), other.words.end(), cmd.words.begin())) {
                by = &other;
                break;
            }
        }
        if (by)
            progress_.push_back("Not listening for \"" + cmd.display + "\": \"" + by->display + "\" fires first");
        else
            kept.push_back(cmd);
    }
    commands_ = std::move(kept);
}

bool SpeechDevice::listensFor(const std::string& phrase) const {
    auto words = canonWords(normalize(phrase));
    for (const auto& cmd : commands_)
        if (cmd.words == words) return true;
    return false;
}

std::vector<std::string> SpeechDevice::matchCommands(const std::string& heard, bool hypothesis) const {
    auto words = canonWords(normalize(heard));
    std::vector<std::string> out;
    size_t i = 0;
    while (i < words.size()) {
        const CommandPhrase* best = nullptr;
        for (const auto& cmd : commands_) {
            if (cmd.words.size() > words.size() - i) continue;
            bool eq = true;
            for (size_t j = 0; j < cmd.words.size(); ++j) {
                if (words[i + j] != cmd.words[j]) {
                    eq = false;
                    break;
                }
            }
            if (!eq) continue;
            if (!best || cmd.words.size() > best->words.size()) best = &cmd;
        }
        if (!best) {
            // A guess has to be commands only. "use the flashlight" still
            // guesses "use", and does not also fire flashlight from the filler.
            if (hypothesis) break;
            ++i;
            continue;
        }
        // GlovePIE's default confidence fires the word as soon as it is heard,
        // even when a longer command is still possible. Confidence 4 and above
        // waits for the finished result instead.
        out.push_back(best->display);
        i += best->words.size();
    }
    return out;
}

void SpeechDevice::beginResultUtterance(unsigned long long audioPos) {
    const bool sameAudio = audioPos != 0 && audioPos == utterancePos_;
    const bool boundary = utteranceClosed_ || expectNewUtterance_ || (audioPos != 0 && audioPos != utterancePos_);
    if (!sameAudio && boundary) {
        firedThisUtterance_.clear();
        expectNewUtterance_ = false;
        utteranceClosed_ = false;
    }
    if (audioPos != 0) utterancePos_ = audioPos;
}

void SpeechDevice::queueMatches(const std::string& heard, int conf, bool hypothesis) {
    auto matched = matchCommands(heard, hypothesis);
    std::unordered_map<std::string, int> counts;
    std::vector<std::string> order;
    for (const auto& m : matched) {
        if (!counts.count(m)) order.push_back(m);
        counts[m]++;
    }
    if (!hypothesis) {
        for (const auto& m : order) {
            finals_[m] = conf;
            int prev = 0;
            auto it = firedThisUtterance_.find(m);
            if (it != firedThisUtterance_.end()) prev = it->second;
            if (counts[m] <= prev && prev > 0)
                progress_.push_back("Recognised, already guessed \"" + m + "\"");
        }
    }
    for (const auto& m : order) {
        int prev = 0;
        auto it = firedThisUtterance_.find(m);
        if (it != firedThisUtterance_.end()) prev = it->second;
        int n = counts[m];
        if (n <= prev) continue;
        firedThisUtterance_[m] = n;
        for (int extra = prev; extra < n; ++extra) {
            pendingPulses_.push_back(PendingPulse{m, conf, eventHeardAt_});
            progress_.push_back(hypothesis ? ("Guess: \"" + m + "\"") : ("Recognised \"" + m + "\""));
        }
    }
}

void SpeechDevice::emitPendingPulse() {
    if (pendingPulses_.empty()) return;
    PendingPulse p = pendingPulses_.front();
    pendingPulses_.erase(pendingPulses_.begin());
    if (p.heardAt >= 0.0) {
        const int ms = static_cast<int>((secondsSinceAudioStart() - p.heardAt) * 1000.0);
        progress_.push_back("Fired \"" + p.phrase + "\" " + std::to_string(ms) + " ms after it was heard");
    }
    pulses_[p.phrase] = p.conf;
    lastRecognized = p.phrase;
    completePhrase = p.phrase;
    partialPhrase = p.phrase;
}

double SpeechDevice::secondsSinceAudioStart() const {
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return double(t.QuadPart - audioStartQpc_) / double(f.QuadPart);
}

void SpeechDevice::ingestRecoText(const std::string& phrase, int conf, bool hypothesis, unsigned long long audioPos) {
    if (phrase.empty()) {
        if (!hypothesis) utteranceClosed_ = true;
        return;
    }
    beginResultUtterance(audioPos);
    if (hypothesis) {
        partialPhrase = phrase;
        hypothese_[phrase] = conf;
    } else {
        completePhrase = phrase;
        inPhrase = false;
    }
    queueMatches(phrase, conf, hypothesis);
    if (!hypothesis) utteranceClosed_ = true;
}

void SpeechDevice::stop() {
    ready_ = false;
    commands_.clear();
    pendingPulses_.clear();
    progress_.clear();
    firedThisUtterance_.clear();
    utterancePos_ = 0;
    utteranceClosed_ = false;
    expectNewUtterance_ = true;
    if (recognizer_) recognizer_->SetRecoState(SPRST_INACTIVE);
    if (grammar_) {
        grammar_->Release();
        grammar_ = nullptr;
    }
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (recognizer_) {
        recognizer_->Release();
        recognizer_ = nullptr;
    }
    if (voice_) {
        voice_->Release();
        voice_ = nullptr;
    }
    if (comInit_) {
        CoUninitialize();
        comInit_ = false;
    }
    status_ = "Speech idle";
    asleep_ = false;
    wentToSleep_ = false;
    wokeUp_ = false;
}

void* SpeechDevice::notifyEvent() const {
    std::lock_guard lock(mu_);
    if (!context_) return nullptr;
    return reinterpret_cast<void*>(context_->GetNotifyEventHandle());
}

void SpeechDevice::poll() {
    std::lock_guard lock(mu_);
    pulses_.clear();
    finals_.clear();
    hypothese_.clear();
    completePhrase.clear();
    partialPhrase.clear();
    if (ready_ && context_ && microphoneEnabled) {
        SPEVENT event{};
        ULONG fetched = 0;
        while (context_->GetEvents(1, &event, &fetched) == S_OK && fetched == 1) {
            if (event.eEventId == SPEI_SOUND_START) {
                inSound = true;
                continue;
            }
            if (event.eEventId == SPEI_SOUND_END) {
                inSound = false;
                continue;
            }
            if (event.eEventId == SPEI_PHRASE_START) {
                inPhrase = true;
                ++phraseSerial_;
                continue;
            }
            const bool hypothesis = event.eEventId == SPEI_HYPOTHESIS;
            const bool accepted = event.eEventId == SPEI_RECOGNITION;
            if ((hypothesis || accepted) && event.lParam) {
                auto* result = reinterpret_cast<ISpRecoResult*>(event.lParam);
                wchar_t* text = nullptr;
                // The guess and the finished result of one phrase share a
                // phrase number. Their start times can differ slightly.
                const unsigned long long audioPos = phraseSerial_;
                int conf = SP_NORMAL_CONFIDENCE;
                SPPHRASE* spPhrase = nullptr;
                if (SUCCEEDED(result->GetPhrase(&spPhrase)) && spPhrase) {
                    conf = static_cast<int>(spPhrase->Rule.Confidence);
                    CoTaskMemFree(spPhrase);
                }
                if (SUCCEEDED(result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &text, nullptr)) && text) {
                    std::string phrase = normalize(wideToUtf8(text));
                    CoTaskMemFree(text);
                    eventHeardAt_ = audioBytesPerSec_ > 0.0
                                        ? double(event.ullAudioStreamOffset) / audioBytesPerSec_
                                        : -1.0;
                    ingestRecoText(phrase, conf, hypothesis, audioPos);
                    eventHeardAt_ = -1.0;
                } else if (accepted) {
                    utteranceClosed_ = true;
                    inPhrase = false;
                }
                result->Release();
            }
        }
        // No Resume here: the rules are SPRS_ACTIVE, so a recognition never
        // pauses the context, and each Resume blocks the frame for 30-50 ms
        // while the engine catches up, which stacked commands up.
    }
    emitPendingPulse();
    applySleepWake();
}

void SpeechDevice::setCommandsForTest(const std::vector<std::string>& phrases) {
    std::lock_guard lock(mu_);
    rebuildCommands(phrases);
    pendingPulses_.clear();
    progress_.clear();
    firedThisUtterance_.clear();
    utterancePos_ = 0;
    utteranceClosed_ = false;
    expectNewUtterance_ = true;
}

void SpeechDevice::hearForTest(const std::string& phrase, int confidence, bool hypothesis, unsigned long long utteranceId) {
    std::lock_guard lock(mu_);
    ingestRecoText(normalize(phrase), confidence, hypothesis, utteranceId);
    emitPendingPulse();
}

std::vector<std::string> SpeechDevice::takeProgress() {
    std::lock_guard lock(mu_);
    std::vector<std::string> out;
    out.swap(progress_);
    return out;
}

void SpeechDevice::injectRecognition(const std::string& phrase, int sapiConfidence, bool partial) {
    std::lock_guard lock(mu_);
    std::string key = normalize(phrase);
    if (partial) {
        hypothese_[key] = sapiConfidence;
        partialPhrase = key;
    } else {
        pulses_[key] = sapiConfidence;
        completePhrase = key;
        lastRecognized = key;
    }
    applySleepWake();
}

void SpeechDevice::setSleepWakeEnabled(bool enabled) {
    std::lock_guard lock(mu_);
    sleepWakeEnabled_ = enabled;
    if (!enabled) {
        asleep_ = false;
        wokeUp_ = false;
        wentToSleep_ = false;
    }
}

bool SpeechDevice::sleeping() const {
    std::lock_guard lock(mu_);
    return sleepWakeEnabled_ && asleep_;
}

bool SpeechDevice::consumeWentToSleep() {
    std::lock_guard lock(mu_);
    bool v = wentToSleep_;
    wentToSleep_ = false;
    return v;
}

bool SpeechDevice::consumeWokeUp() {
    std::lock_guard lock(mu_);
    bool v = wokeUp_;
    wokeUp_ = false;
    return v;
}

std::string SpeechDevice::status() const {
    std::lock_guard lock(mu_);
    if (sleepWakeEnabled_ && asleep_) return status_ + " (sleeping — say wake up)";
    return status_;
}

bool SpeechDevice::heardSleepCommandLocked() const {
    for (const auto& [heard, _] : pulses_) {
        std::string f = foldPhrase(heard);
        if (f == "go to sleep" || phraseContains(heard, "go to sleep")) return true;
    }
    return false;
}

bool SpeechDevice::heardWakeCommandLocked() const {
    for (const auto& [heard, _] : pulses_) {
        std::string f = foldPhrase(heard);
        if (f == "wake up" || f == "wakeup" || phraseContains(heard, "wake up")) return true;
    }
    return false;
}

void SpeechDevice::applySleepWake() {
    if (!sleepWakeEnabled_) return;
    const bool wake = heardWakeCommandLocked();
    const bool sleep = heardSleepCommandLocked();
    if (asleep_) {
        if (wake) {
            asleep_ = false;
            wokeUp_ = true;
            lastRecognized = "wake up";
        }
        return;
    }
    if (sleep && !wake) {
        asleep_ = true;
        wentToSleep_ = true;
        lastRecognized = "go to sleep";
    }
}

bool SpeechDevice::said(const std::string& phrase, int confidence) {
    std::lock_guard lock(mu_);
    if (!microphoneEnabled || pushToNotTalk) return false;
    if (sleepWakeEnabled_ && asleep_) return false;
    std::string key = spokenCanonical(normalize(phrase));
    int required = confidence;
    int words = 1;
    for (char c : key)
        if (c == ' ') ++words;
    // One-word commands like "walk" / "halt" are easy for SAPI to reject at default confidence.
    if (words <= 1 && required >= 2 && required <= 3) required = 1;
    auto match = [&](const std::unordered_map<std::string, int>& map, bool partial) {
        for (const auto& [heard, conf] : map) {
            const bool same = spokenCanonical(foldPhrase(heard)) == key || foldPhrase(heard) == key ||
                              heard == normalize(phrase) || phraseContains(heard, key);
            if (same && (partial ? confidenceOk(required, conf, true) : true)) return true;
        }
        return false;
    };
    // Confidence 4 and above is GlovePIE's "wait until the phrase is finished".
    // Lower confidence fires on the guess, as soon as the word has been heard.
    if (required >= 4) return match(finals_, false);
    if (match(pulses_, false)) return true;
    return false;
}

void SpeechDevice::say(const std::string& text) {
    if (!voice_ || text.empty()) return;
    std::wstring w = utf8ToWide(text);
    voice_->Speak(w.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

void SpeechDevice::setSpeechText(const std::string& text) {
    if (text == lastSpeechText_) return;
    lastSpeechText_ = text;
    say(text);
}

Value SpeechDevice::getProperty(const std::string& object, const std::string& name) {
    std::string obj = normalize(object);
    std::string n = normalize(name);
    n.erase(std::remove(n.begin(), n.end(), '_'), n.end());
    if (obj == "microphone") {
        if (n == "enabled") return Value::boolean(microphoneEnabled);
        if (n == "completephrase") return Value::string(completePhrase);
        if (n == "partialphrase") return Value::string(partialPhrase);
        if (n == "audiolevel") return Value::number(0);
        if (n == "inphrase") return Value::boolean(inPhrase);
        if (n == "insound") return Value::boolean(inSound);
        if (n == "pushtonottalk") return Value::boolean(pushToNotTalk);
        if (n == "toofast" || n == "tooslow" || n == "tooloud" || n == "tooquiet" || n == "heardnoise")
            return Value::boolean(false);
    }
    if (obj == "speech") {
        if (n == "text" || n.size() > 5 && n.substr(n.size() - 5) == ".text") return Value::string(lastSpeechText_);
    }
    return Value::null();
}

void SpeechDevice::setProperty(const std::string& object, const std::string& name, const Value& value) {
    std::string obj = normalize(object);
    std::string n = normalize(name);
    n.erase(std::remove(n.begin(), n.end(), '_'), n.end());
    if (obj == "microphone" && n == "enabled") {
        microphoneEnabled = value.isTruthy();
        return;
    }
    if (obj == "microphone" && n == "pushtonottalk") {
        pushToNotTalk = value.isTruthy();
        return;
    }
    if (obj == "speech" && (n == "text" || n.size() > 5 && n.substr(n.size() - 5) == ".text")) {
        setSpeechText(value.asString());
    }
}
