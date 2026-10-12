#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "OscDevice.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int oscIndex(const std::string& root) {
    if (root == "osc") return 1;
    if (root.size() > 3 && root.rfind("osc", 0) == 0 && std::isdigit(static_cast<unsigned char>(root[3])))
        return std::max(1, std::stoi(root.substr(3)));
    return 0;
}

std::string toOscAddr(std::string rest) {
    if (rest.empty()) return "/";
    for (char& c : rest)
        if (c == '.') c = '/';
    if (rest[0] != '/') rest = "/" + rest;
    return rest;
}

void pad4(std::string& s) {
    while (s.size() % 4) s.push_back('\0');
}

std::string encodeOsc(const std::string& address, const std::vector<Value>& args) {
    std::string addr = address;
    if (addr.empty() || addr[0] != '/') addr = "/" + addr;
    pad4(addr);
    std::string tags = ",";
    std::string data;
    auto putInt = [&](int v) {
        unsigned u = static_cast<unsigned>(v);
        data.push_back(static_cast<char>((u >> 24) & 0xFF));
        data.push_back(static_cast<char>((u >> 16) & 0xFF));
        data.push_back(static_cast<char>((u >> 8) & 0xFF));
        data.push_back(static_cast<char>(u & 0xFF));
    };
    auto putFloat = [&](float f) {
        unsigned u = 0;
        std::memcpy(&u, &f, 4);
        putInt(static_cast<int>(u));
    };
    for (const auto& a : args) {
        if (a.kind == ValueKind::String) {
            tags += 's';
            data += a.s;
            data.push_back('\0');
            pad4(data);
        } else if (a.kind == ValueKind::Bool) {
            tags += a.b ? 'T' : 'F';
        } else if (a.kind == ValueKind::Number && std::floor(a.n) == a.n) {
            tags += 'i';
            putInt(static_cast<int>(a.n));
        } else {
            tags += 'f';
            putFloat(static_cast<float>(a.asNumber()));
        }
    }
    pad4(tags);
    return addr + tags + data;
}
}

struct OscDevice::Impl {
    bool wsa = false;
    SOCKET sock = INVALID_SOCKET;
    SOCKET listen = INVALID_SOCKET;
    int listenPort = 0;
    struct Slot {
        int port = 54934;
        int listenPort = 0;
        bool broadcast = false;
        bool listening = false;
        std::string ip = "127.0.0.1";
        std::unordered_map<std::string, Value> inbox;
    };
    Slot slots[8];
    std::mutex mu;

    Impl() {
        WSADATA d{};
        wsa = WSAStartup(MAKEWORD(2, 2), &d) == 0;
        if (wsa) sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock != INVALID_SOCKET) {
            BOOL yes = TRUE;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&yes), sizeof(yes));
        }
    }
    ~Impl() {
        if (listen != INVALID_SOCKET) closesocket(listen);
        if (sock != INVALID_SOCKET) closesocket(sock);
        if (wsa) WSACleanup();
    }

    void ensureListen(int port) {
        if (dryRun || port <= 0) return;
        if (listen != INVALID_SOCKET && listenPort == port) return;
        if (listen != INVALID_SOCKET) closesocket(listen);
        listen = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        listenPort = port;
        if (listen == INVALID_SOCKET) return;
        u_long nb = 1;
        ioctlsocket(listen, FIONBIO, &nb);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<u_short>(port));
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(listen, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    }

    bool dryRun = false;

    void sendTo(const std::string& ip, int port, const std::string& packet) {
        if (dryRun || sock == INVALID_SOCKET || port <= 0) return;
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<u_short>(port));
        if (ip == "broadcast" || ip.empty())
            a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        else
            InetPtonA(AF_INET, ip.c_str(), &a.sin_addr);
        sendto(sock, packet.data(), static_cast<int>(packet.size()), 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    }

    void pollListen() {
        if (listen == INVALID_SOCKET) return;
        char buf[2048];
        sockaddr_in from{};
        int flen = sizeof(from);
        int n = recvfrom(listen, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &flen);
        if (n <= 0) return;
        if (buf[0] != '/') return;
        int i = 0;
        while (i < n && buf[i]) ++i;
        std::string addr(buf, buf + i);
        for (char& c : addr)
            if (c == '/') c = '.';
        if (!addr.empty() && addr[0] == '.') addr.erase(addr.begin());
        while (i % 4) ++i;
        std::lock_guard<std::mutex> lock(mu);
        for (auto& s : slots) {
            if (!s.listening) continue;
            s.inbox[lower(addr)] = Value::number(1);
        }
    }
};

OscDevice::OscDevice() : impl_(new Impl) {}
OscDevice::~OscDevice() { delete impl_; }

bool OscDevice::handles(const std::string& root) const { return oscIndex(lower(root)) > 0; }

Value OscDevice::get(const std::string& root, const std::string& rest) {
    impl_->dryRun = dryRun;
    impl_->pollListen();
    int idx = oscIndex(lower(root));
    if (idx < 1 || idx > 8) return Value::boolean(false);
    auto& s = impl_->slots[idx - 1];
    std::string p = lower(rest);
    if (p == "port") return Value::number(s.port);
    if (p == "listenport") return Value::number(s.listenPort);
    if (p == "broadcast") return Value::boolean(s.broadcast);
    if (p == "listening") return Value::boolean(s.listening);
    if (p == "ip") return Value::string(s.ip);
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto it = s.inbox.find(p);
    if (it != s.inbox.end()) return it->second;
    return Value::number(0);
}

void OscDevice::set(const std::string& root, const std::string& rest, const Value& value) {
    impl_->dryRun = dryRun;
    int idx = oscIndex(lower(root));
    if (idx < 1 || idx > 8) return;
    auto& s = impl_->slots[idx - 1];
    std::string p = lower(rest);
    if (p == "port") {
        s.port = static_cast<int>(value.asNumber());
        return;
    }
    if (p == "listenport") {
        s.listenPort = static_cast<int>(value.asNumber());
        return;
    }
    if (p == "broadcast") {
        s.broadcast = value.isTruthy();
        return;
    }
    if (p == "ip") {
        s.ip = value.asString();
        return;
    }
    if (p == "listening") {
        s.listening = value.isTruthy();
        if (s.listening) impl_->ensureListen(s.listenPort > 0 ? s.listenPort : s.port);
        return;
    }
    std::string addr = toOscAddr(rest);
    impl_->sendTo(s.broadcast ? "broadcast" : s.ip, s.port, encodeOsc(addr, {value}));
}

void OscDevice::send(const std::string& ip, int port, const std::string& address, const std::vector<Value>& args) {
    impl_->dryRun = dryRun;
    impl_->sendTo(ip, port, encodeOsc(address, args));
}

void OscDevice::poll() {
    impl_->dryRun = dryRun;
    impl_->pollListen();
}

void OscDevice::reset() {
    for (auto& s : impl_->slots) s = {};
}
