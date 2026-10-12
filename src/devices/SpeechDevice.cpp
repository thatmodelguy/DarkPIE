#include "SpeechDevice.h"

#include <Windows.h>
#include <sapi.h>

#include <algorithm>
#include <cctype>
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

    context_->SetNotifyWin32Event();
    context_->SetMaxAlternates(8);
    recognizer_->SetPropertyNum(L"CFGConfidenceRejectionThreshold", 0);
    ULONGLONG interest = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS) | SPFEI(SPEI_FALSE_RECOGNITION);
    context_->SetInterest(interest, interest);

    hr = context_->CreateGrammar(1, &grammar_);
    if (FAILED(hr) || !grammar_) {
        error = "Could not create SAPI grammar.";
        status_ = error;
        return false;
    }

    grammar_->ResetGrammar(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    knownPhrases_.clear();
    asleep_ = false;
    wentToSleep_ = false;
    wokeUp_ = false;
    std::vector<std::string> listen = phrases;
    if (sleepWakeEnabled_) {
        listen.push_back("go to sleep");
        listen.push_back("wake up");
        listen.push_back("wakeup");
    }
    ULONG ruleId = 1;
    auto addPhraseRule = [&](const std::string& form, float weight) -> HRESULT {
        if (form.empty()) return S_OK;
        std::wstring name = L"cmd" + std::to_wstring(ruleId);
        SPSTATEHANDLE hRule = nullptr;
        HRESULT r = grammar_->GetRule(name.c_str(), ruleId, SPRAF_TopLevel | SPRAF_Active, TRUE, &hRule);
        if (FAILED(r) || !hRule) return r;
        std::wstring w = utf8ToWide(form);
        r = grammar_->AddWordTransition(hRule, nullptr, w.c_str(), L" ", SPWT_LEXICAL, weight, nullptr);
        ++ruleId;
        return r;
    };
    if (listen.empty()) {
        hr = addPhraseRule("darkpie idle", 1.0f);
    } else {
        for (const auto& p : listen) {
            std::string n = normalize(p);
            knownPhrases_.insert(n);
            knownPhrases_.insert(spokenCanonical(n));
            int words = 1;
            for (char c : n)
                if (c == ' ') ++words;
            const float weight = (words <= 1) ? 50.0f : 1.0f;
            for (const auto& form : grammarForms(p)) {
                hr = addPhraseRule(form, weight);
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

    ready_ = true;
    if (listen.empty()) status_ = "Speech ready (no said() phrases)";
    else if (sleepWakeEnabled_)
        status_ = "Listening for " + std::to_string(phrases.size()) +
                  " script commands + sleep/wake";
    else
        status_ = "Listening for " + std::to_string(phrases.size()) + " script commands";
    return true;
}

bool SpeechDevice::isKnownPhrase(const std::string& heard) const {
    std::string raw = foldPhrase(normalize(heard));
    std::string n = spokenCanonical(raw);
    return knownPhrases_.count(n) > 0 || knownPhrases_.count(raw) > 0;
}

bool SpeechDevice::tryPulse(const std::string& phrase, int conf) {
    auto canon = spokenCanonical(foldPhrase(phrase));
    auto nowTs = std::chrono::steady_clock::now();
    auto prev = lastPulseAt_.find(canon);
    if (prev != lastPulseAt_.end() && (nowTs - prev->second) < std::chrono::milliseconds(1000))
        return false;
    pulses_[phrase] = conf;
    lastPulseAt_[canon] = nowTs;
    lastRecognized = phrase;
    completePhrase = phrase;
    return true;
}

void SpeechDevice::pulseMatches(const std::string& heard, int conf, bool strict) {
    std::vector<std::string> hits;
    const std::string folded = foldPhrase(normalize(heard));
    for (const auto& known : knownPhrases_) {
        const std::string kf = foldPhrase(known);
        if (strict) {
            if (folded != kf) continue;
        } else if (!phraseContains(heard, known)) {
            continue;
        }
        hits.push_back(kf);
    }
    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
    std::sort(hits.begin(), hits.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    std::vector<std::string> kept;
    for (const auto& h : hits) {
        bool swallowed = false;
        for (const auto& k : kept) {
            if (k != h && phraseContains(k, h) && k.size() > h.size()) {
                swallowed = true;
                break;
            }
        }
        if (!swallowed) kept.push_back(h);
    }
    for (const auto& h : kept) tryPulse(h, conf);
}

void SpeechDevice::ingestRecoText(const std::string& phrase, int conf, bool hypothesis) {
    if (hypothesis) {
        partialPhrase = phrase;
        hypothese_[phrase] = conf;
        pulseMatches(phrase, conf, true);
        return;
    }
    completePhrase = phrase;
    pulseMatches(phrase, conf, false);
    if (isKnownPhrase(phrase)) tryPulse(phrase, conf);
}

void SpeechDevice::stop() {
    ready_ = false;
    knownPhrases_.clear();
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

void SpeechDevice::poll() {
    std::lock_guard lock(mu_);
    pulses_.clear();
    hypothese_.clear();
    completePhrase.clear();
    partialPhrase.clear();
    if (!ready_ || !context_ || !microphoneEnabled) return;

    SPEVENT event{};
    ULONG fetched = 0;
    bool sawFinal = false;
    while (context_->GetEvents(1, &event, &fetched) == S_OK && fetched == 1) {
        const bool hypothesis = event.eEventId == SPEI_HYPOTHESIS || event.eEventId == SPEI_FALSE_RECOGNITION;
        const bool accepted = event.eEventId == SPEI_RECOGNITION;
        if ((hypothesis || accepted) && event.lParam) {
            auto* result = reinterpret_cast<ISpRecoResult*>(event.lParam);
            auto takeText = [&](ISpPhrase* phraseObj) {
                if (!phraseObj) return;
                wchar_t* text = nullptr;
                if (SUCCEEDED(phraseObj->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &text, nullptr)) && text) {
                    std::string phrase = normalize(wideToUtf8(text));
                    CoTaskMemFree(text);
                    SPPHRASE* spPhrase = nullptr;
                    int conf = SP_NORMAL_CONFIDENCE;
                    if (SUCCEEDED(phraseObj->GetPhrase(&spPhrase)) && spPhrase) {
                        conf = static_cast<int>(spPhrase->Rule.Confidence);
                        CoTaskMemFree(spPhrase);
                    }
                    ingestRecoText(phrase, conf, hypothesis);
                }
            };
            takeText(result);
            ISpPhraseAlt* alts[8]{};
            ULONG got = 0;
            if (SUCCEEDED(result->GetAlternates(0, static_cast<ULONG>(-1), 8, alts, &got))) {
                for (ULONG i = 0; i < got; ++i) {
                    takeText(alts[i]);
                    if (alts[i]) alts[i]->Release();
                }
            }
            result->Release();
            if (accepted) sawFinal = true;
        }
    }
    if (sawFinal && recognizer_) {
        recognizer_->SetRecoState(SPRST_INACTIVE);
        recognizer_->SetRecoState(SPRST_ACTIVE_ALWAYS);
    }
    applySleepWake();
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
