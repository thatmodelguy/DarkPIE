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

std::vector<std::string> canonWords(const std::string& phrase) {
    std::string folded = foldPhrase(phrase);
    auto words = splitWords(folded);
    for (auto& w : words) w = spokenCanonical(w);
    return words;
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
    context_->SetMaxAlternates(3);
    recognizer_->SetPropertyNum(L"CFGConfidenceRejectionThreshold", 0);
    // Short trailing silence so a finished word can be followed by the next one.
    // Set once, before the engine is running. Changing these live made commands lag.
    recognizer_->SetPropertyNum(L"ComplexResponseSpeed", 10000);
    recognizer_->SetPropertyNum(L"ResponseSpeed", 90);
    recognizer_->SetPropertyNum(L"ComplexResponseSpeed", 200);
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
    ULONG ruleId = 1;
    auto addPhraseRule = [&](const std::string& form, float weight) -> HRESULT {
        if (form.empty()) return S_OK;
        std::wstring name = L"cmd" + std::to_wstring(ruleId);
        SPSTATEHANDLE hRule = nullptr;
        HRESULT r = grammar_->GetRule(name.c_str(), ruleId, SPRAF_TopLevel | SPRAF_Active, TRUE, &hRule);
        if (FAILED(r) || !hRule) return r;
        std::wstring w = utf8ToWide(form);
        // The property is pushed as soon as this command is unambiguous, so a
        // hypothesis can name it before trailing silence closes the utterance.
        SPPROPERTYINFO prop{};
        prop.pszName = L"cmd";
        prop.ulId = ruleId;
        prop.pszValue = w.c_str();
        prop.vValue.vt = VT_EMPTY;
        r = grammar_->AddWordTransition(hRule, nullptr, w.c_str(), L" ", SPWT_LEXICAL, weight, &prop);
        if (FAILED(r)) r = grammar_->AddWordTransition(hRule, nullptr, w.c_str(), L" ", SPWT_LEXICAL, weight, nullptr);
        ++ruleId;
        return r;
    };
    if (listen.empty()) {
        hr = addPhraseRule("darkpie idle", 1.0f);
    } else {
        for (const auto& p : listen) {
            std::string n = normalize(p);
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

    // Dictation hypotheses arrive word by word, before the command grammar
    // waits for trailing silence. Guesses come from those words; the command
    // grammar still supplies the accurate final result.
    if (SUCCEEDED(context_->CreateGrammar(2, &dictation_)) && dictation_) {
        if (FAILED(dictation_->LoadDictation(nullptr, SPLO_STATIC))) {
            dictation_->Release();
            dictation_ = nullptr;
        } else {
            dictation_->SetDictationState(SPRS_ACTIVE);
        }
    }

    context_->SetContextState(SPCS_ENABLED);
    recognizer_->SetRecoState(SPRST_ACTIVE_ALWAYS);
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
}

bool SpeechDevice::extendsLonger(const CommandPhrase& cmd) const {
    for (const auto& other : commands_) {
        if (other.words.size() <= cmd.words.size()) continue;
        bool prefix = true;
        for (size_t i = 0; i < cmd.words.size(); ++i) {
            if (other.words[i] != cmd.words[i]) {
                prefix = false;
                break;
            }
        }
        if (prefix) return true;
    }
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
        const bool atEnd = i + best->words.size() == words.size();
        // "go" while the script also has "go up" stays a guess until more
        // words arrive or the utterance closes. "use" with nothing longer fires now.
        if (hypothesis && atEnd && extendsLonger(*best)) break;
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
            pendingPulses_.push_back(PendingPulse{m, conf});
            progress_.push_back(hypothesis ? ("Guess: \"" + m + "\"") : ("Recognised \"" + m + "\""));
        }
    }
}

void SpeechDevice::emitPendingPulse() {
    if (pendingPulses_.empty()) return;
    PendingPulse p = pendingPulses_.front();
    pendingPulses_.erase(pendingPulses_.begin());
    pulses_[p.phrase] = p.conf;
    lastRecognized = p.phrase;
    completePhrase = p.phrase;
    partialPhrase = p.phrase;
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
    if (dictation_) {
        dictation_->Release();
        dictation_ = nullptr;
    }
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
    if (ready_ && context_ && microphoneEnabled) {
        SPEVENT event{};
        ULONG fetched = 0;
        while (context_->GetEvents(1, &event, &fetched) == S_OK && fetched == 1) {
            if (event.eEventId == SPEI_SOUND_START) {
                inSound = true;
                expectNewUtterance_ = true;
                continue;
            }
            if (event.eEventId == SPEI_SOUND_END) {
                inSound = false;
                continue;
            }
            if (event.eEventId == SPEI_PHRASE_START) {
                inPhrase = true;
                expectNewUtterance_ = true;
                continue;
            }
            const bool hypothesis = event.eEventId == SPEI_HYPOTHESIS;
            const bool accepted = event.eEventId == SPEI_RECOGNITION;
            if ((hypothesis || accepted) && event.lParam) {
                auto* result = reinterpret_cast<ISpRecoResult*>(event.lParam);
                wchar_t* text = nullptr;
                unsigned long long audioPos = 0;
                int conf = SP_NORMAL_CONFIDENCE;
                SPPHRASE* spPhrase = nullptr;
                if (SUCCEEDED(result->GetPhrase(&spPhrase)) && spPhrase) {
                    conf = static_cast<int>(spPhrase->Rule.Confidence);
                    audioPos = spPhrase->ftStartTime != 0 ? spPhrase->ftStartTime : spPhrase->ullAudioStreamPosition;
                    CoTaskMemFree(spPhrase);
                }
                if (SUCCEEDED(result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &text, nullptr)) && text) {
                    std::string phrase = normalize(wideToUtf8(text));
                    CoTaskMemFree(text);
                    ingestRecoText(phrase, conf, hypothesis, audioPos);
                } else if (accepted) {
                    utteranceClosed_ = true;
                    inPhrase = false;
                }
                result->Release();
            }
        }
        // A recognition pauses the context until Resume. Restarting the
        // recognizer here used to drop the start of the next word.
        context_->Resume(0);
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
