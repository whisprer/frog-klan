#include "net.h"
#include <chrono>
#include <cstring>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  static bool wsa_inited = false;
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

static void sock_close(int s) {
#if defined(_WIN32)
    closesocket((SOCKET)s);
#else
    close(s);
#endif
}

UdpClient::UdpClient() {}
UdpClient::~UdpClient() { close(); }

bool UdpClient::open() {
#if defined(_WIN32)
    if (!wsa_inited) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2,2), &w) != 0) return false;
        wsa_inited = true;
    }
#endif
    if (sock_ != -1) return true;
    int s = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return false;
    sock_ = s;
    return true;
}

void UdpClient::close() {
    if (sock_ != -1) {
        sock_close(sock_);
        sock_ = -1;
    }
}

static bool resolve_ipv4(const std::string& host, uint16_t port, sockaddr_in* out) {
    std::memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(port);

    // Try literal IP first
    if (inet_pton(AF_INET, host.c_str(), &out->sin_addr) == 1) return true;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    auto* a = (sockaddr_in*)res->ai_addr;
    out->sin_addr = a->sin_addr;
    freeaddrinfo(res);
    return true;
}

UdpReply UdpClient::request(const UdpEndpoint& ep, const std::string& payload, int timeout_ms) {
    UdpReply r;
    if (sock_ == -1 && !open()) return r;

    sockaddr_in dst{};
    if (!resolve_ipv4(ep.host, ep.port, &dst)) return r;

#if defined(_WIN32)
    DWORD tv = (DWORD)timeout_ms;
    setsockopt((SOCKET)sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    auto t0 = std::chrono::steady_clock::now();
    int sent = sendto(sock_, payload.data(), (int)payload.size(), 0, (sockaddr*)&dst, sizeof(dst));
    if (sent <= 0) return r;

    char buf[65536];
    sockaddr_in src{};
#if defined(_WIN32)
    int slen = sizeof(src);
#else
    socklen_t slen = sizeof(src);
#endif
    int got = recvfrom(sock_, buf, (int)sizeof(buf)-1, 0, (sockaddr*)&src, &slen);
    auto t1 = std::chrono::steady_clock::now();

    if (got <= 0) return r;
    buf[got] = 0;

    char ip[64];
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));

    r.ok = true;
    r.data.assign(buf, got);
    r.peer_ip = ip;
    r.peer_port = ntohs(src.sin_port);
    r.elapsed_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    return r;
}
