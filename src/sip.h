#pragma once
#include <string>
#include <map>
#include <cstdint>

struct SipAuthChallenge {
    bool ok = false;
    std::string realm;
    std::string nonce;
    std::string qop;      // e.g. "auth"
    std::string opaque;
    std::string algorithm; // usually "MD5"
};

struct SipResponse {
    int status = 0;
    std::string reason;
    std::map<std::string, std::string> headers_lc; // lowercased header name -> value
    std::string raw;
};

struct SipProbeResult {
    bool ok = false;
    int status = 0;
    int rtt_ms = -1;
    std::string peer_ip;
    uint16_t peer_port = 0;
    std::string note;
};

SipResponse parse_sip_response(const std::string& raw);
SipAuthChallenge parse_www_authenticate_digest(const SipResponse& resp);

std::string make_sip_options(
    const std::string& host, uint16_t port,
    const std::string& from_uri,
    const std::string& to_uri,
    const std::string& user_agent,
    const std::string& call_id,
    int cseq,
    const std::string& branch,
    const std::string& local_tag
);

std::string make_sip_register(
    const std::string& host, uint16_t port,
    const std::string& aor_uri,
    const std::string& contact_uri,
    const std::string& user_agent,
    const std::string& call_id,
    int cseq,
    const std::string& branch,
    const std::string& local_tag,
    int expires_seconds,
    const std::string& authorization_header // "" if none
);

std::string build_digest_authorization(
    const std::string& method,
    const std::string& uri,
    const std::string& username,
    const std::string& password,
    const SipAuthChallenge& ch,
    const std::string& cnonce,
    const std::string& nc_hex8
);
