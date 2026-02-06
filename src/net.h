#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct UdpEndpoint {
    std::string host;
    uint16_t port;
};

struct UdpReply {
    bool ok = false;
    std::string data;
    std::string peer_ip;
    uint16_t peer_port = 0;
    int elapsed_ms = -1;
};

class UdpClient {
public:
    UdpClient();
    ~UdpClient();

    bool open();
    void close();

    // Sends UDP and waits for a single reply. Retries are handled by caller.
    UdpReply request(const UdpEndpoint& ep, const std::string& payload, int timeout_ms);

private:
    int sock_ = -1;
};
