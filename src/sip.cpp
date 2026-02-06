#include "sip.h"
#include "md5.h"
#include <algorithm>
#include <sstream>
#include <random>

static inline std::string trim(std::string s) {
    auto notsp = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

static inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

SipResponse parse_sip_response(const std::string& raw) {
    SipResponse r;
    r.raw = raw;

    std::istringstream iss(raw);
    std::string line;
    if (!std::getline(iss, line)) return r;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // "SIP/2.0 200 OK"
    {
        std::istringstream sl(line);
        std::string sipver;
        sl >> sipver >> r.status;
        std::getline(sl, r.reason);
        r.reason = trim(r.reason);
    }

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        auto name = lower(trim(line.substr(0, pos)));
        auto val  = trim(line.substr(pos + 1));
        // Merge duplicates naïvely with comma (fine for our use)
        if (r.headers_lc.count(name)) r.headers_lc[name] += ", " + val;
        else r.headers_lc[name] = val;
    }
    return r;
}

static std::map<std::string, std::string> parse_kv_params(const std::string& s) {
    // input like: Digest realm="x", nonce="y", qop="auth"
    std::map<std::string, std::string> m;
    auto p = s;
    auto dig = lower(p);
    auto idx = dig.find("digest");
    if (idx != std::string::npos) p = p.substr(idx + 6);

    size_t i = 0;
    while (i < p.size()) {
        while (i < p.size() && (p[i] == ' ' || p[i] == '\t' || p[i] == ',')) i++;
        size_t k0 = i;
        while (i < p.size() && p[i] != '=' && p[i] != ',' ) i++;
        if (i >= p.size() || p[i] != '=') break;
        std::string key = lower(trim(p.substr(k0, i - k0)));
        i++; // '='
        while (i < p.size() && (p[i] == ' ' || p[i] == '\t')) i++;

        std::string val;
        if (i < p.size() && p[i] == '"') {
            i++;
            size_t v0 = i;
            while (i < p.size() && p[i] != '"') i++;
            val = p.substr(v0, i - v0);
            if (i < p.size() && p[i] == '"') i++;
        } else {
            size_t v0 = i;
            while (i < p.size() && p[i] != ',') i++;
            val = trim(p.substr(v0, i - v0));
        }
        if (!key.empty()) m[key] = val;
    }
    return m;
}

SipAuthChallenge parse_www_authenticate_digest(const SipResponse& resp) {
    SipAuthChallenge ch;
    auto it = resp.headers_lc.find("www-authenticate");
    if (it == resp.headers_lc.end()) return ch;

    auto params = parse_kv_params(it->second);
    if (params.count("realm") && params.count("nonce")) {
        ch.ok = true;
        ch.realm = params["realm"];
        ch.nonce = params["nonce"];
        if (params.count("qop")) ch.qop = params["qop"];
        if (params.count("opaque")) ch.opaque = params["opaque"];
        if (params.count("algorithm")) ch.algorithm = params["algorithm"];
        else ch.algorithm = "MD5";
    }
    return ch;
}

static std::string rand_hex(size_t nbytes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    std::string out;
    out.reserve(nbytes*2);
    static const char* h="0123456789abcdef";
    for (size_t i=0;i<nbytes;i++){
        int v = dis(gen);
        out.push_back(h[(v>>4)&15]);
        out.push_back(h[v&15]);
    }
    return out;
}

std::string build_digest_authorization(
    const std::string& method,
    const std::string& uri,
    const std::string& username,
    const std::string& password,
    const SipAuthChallenge& ch,
    const std::string& cnonce,
    const std::string& nc_hex8
) {
    // RFC 2617 / SIP Digest MD5
    // HA1 = MD5(username:realm:password)
    // HA2 = MD5(method:uri)
    // response = MD5(HA1:nonce:HA2) OR if qop: MD5(HA1:nonce:nc:cnonce:qop:HA2)
    std::string ha1 = MD5::md5_hex(username + ":" + ch.realm + ":" + password);
    std::string ha2 = MD5::md5_hex(method + ":" + uri);

    std::string resp;
    std::string qop = ch.qop;
    // If qop has multiple, pick auth if present
    if (!qop.empty()) {
        auto ql = qop;
        std::transform(ql.begin(), ql.end(), ql.begin(), ::tolower);
        if (ql.find("auth") != std::string::npos) qop = "auth";
        else qop = trim(qop);
        resp = MD5::md5_hex(ha1 + ":" + ch.nonce + ":" + nc_hex8 + ":" + cnonce + ":" + qop + ":" + ha2);
    } else {
        resp = MD5::md5_hex(ha1 + ":" + ch.nonce + ":" + ha2);
    }

    std::ostringstream o;
    o << "Digest username=\"" << username << "\""
      << ", realm=\"" << ch.realm << "\""
      << ", nonce=\"" << ch.nonce << "\""
      << ", uri=\"" << uri << "\""
      << ", response=\"" << resp << "\""
      << ", algorithm=MD5";

    if (!ch.opaque.empty()) o << ", opaque=\"" << ch.opaque << "\"";
    if (!qop.empty()) o << ", qop=" << qop << ", nc=" << nc_hex8 << ", cnonce=\"" << cnonce << "\"";
    return o.str();
}

static std::string sip_uri_hostport(const std::string& host, uint16_t port) {
    std::ostringstream o;
    o << "sip:" << host;
    if (port != 5060) o << ":" << port;
    return o.str();
}

static std::string ensure_tag(std::string s) {
    if (s.find(";tag=") != std::string::npos) return s;
    return s + ";tag=" + rand_hex(6);
}

std::string make_sip_options(
    const std::string& host, uint16_t port,
    const std::string& from_uri,
    const std::string& to_uri,
    const std::string& user_agent,
    const std::string& call_id,
    int cseq,
    const std::string& branch,
    const std::string& local_tag
) {
    const std::string req_uri = sip_uri_hostport(host, port);
    std::ostringstream o;
    o << "OPTIONS " << req_uri << " SIP/2.0\r\n";
    o << "Via: SIP/2.0/UDP " << host << ":" << port << ";branch=" << branch << "\r\n";
    o << "Max-Forwards: 70\r\n";
    o << "From: <" << from_uri << ">;tag=" << local_tag << "\r\n";
    o << "To: <" << to_uri << ">\r\n";
    o << "Call-ID: " << call_id << "\r\n";
    o << "CSeq: " << cseq << " OPTIONS\r\n";
    o << "Contact: <" << from_uri << ">\r\n";
    o << "User-Agent: " << user_agent << "\r\n";
    o << "Accept: application/sdp\r\n";
    o << "Content-Length: 0\r\n\r\n";
    return o.str();
}

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
    const std::string& authorization_header
) {
    const std::string req_uri = sip_uri_hostport(host, port);
    std::ostringstream o;
    o << "REGISTER " << req_uri << " SIP/2.0\r\n";
    o << "Via: SIP/2.0/UDP " << host << ":" << port << ";branch=" << branch << "\r\n";
    o << "Max-Forwards: 70\r\n";
    o << "From: <" << aor_uri << ">;tag=" << local_tag << "\r\n";
    o << "To: <" << aor_uri << ">\r\n";
    o << "Call-ID: " << call_id << "\r\n";
    o << "CSeq: " << cseq << " REGISTER\r\n";
    o << "Contact: <" << contact_uri << ">\r\n";
    o << "Expires: " << expires_seconds << "\r\n";
    if (!authorization_header.empty()) o << "Authorization: " << authorization_header << "\r\n";
    o << "User-Agent: " << user_agent << "\r\n";
    o << "Content-Length: 0\r\n\r\n";
    return o.str();
}
