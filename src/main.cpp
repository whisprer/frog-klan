#include "net.h"
#include "sip.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <random>

namespace fs = std::filesystem;

static const char* APP_NAME = "frogklan";
static const char* APP_VERSION = "1.0.0-qa";

static std::string os_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

static fs::path app_data_dir() {
#if defined(_WIN32)
    const char* p = std::getenv("APPDATA");
    return fs::path(p ? p : fs::temp_directory_path().string()) / APP_NAME;
#elif defined(__APPLE__)
    const char* h = std::getenv("HOME");
    return fs::path(h ? h : "/tmp") / "Library/Application Support" / APP_NAME;
#else
    const char* h = std::getenv("HOME");
    return fs::path(h ? h : "/tmp") / (std::string(".") + APP_NAME);
#endif
}

static std::string rand_hex(size_t nbytes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0,255);
    static const char* h="0123456789abcdef";
    std::string s; s.reserve(nbytes*2);
    for (size_t i=0;i<nbytes;i++){
        int v=dis(gen);
        s.push_back(h[(v>>4)&15]); s.push_back(h[v&15]);
    }
    return s;
}

static void usage() {
    std::cout <<
"frogklan (SIP QA) " << APP_VERSION << "\n"
"Usage:\n"
"  frogklan qa --host <sip.host> [--port 5060] [--timeout 1200] [--retries 2]\n"
"              --from <sip:you@domain> --to <sip:dest@domain>\n"
"              [--register --aor <sip:you@domain> --contact <sip:you@host>\n"
"               --user <u> --pass <p> --expires 300]\n"
"\n"
"Examples:\n"
"  frogklan qa --host sip.example.com --from sip:qa@ex.com --to sip:qa@ex.com\n"
"  frogklan qa --host sip.example.com --from sip:qa@ex.com --to sip:qa@ex.com \\\n"
"      --register --aor sip:1001@example.com --contact sip:1001@sip.example.com \\\n"
"      --user 1001 --pass secret --expires 120\n";
}

static std::string json_escape(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for (char c: s){
        switch(c){
            case '\\': o += "\\\\"; break;
            case '"': o += "\\\""; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default: o.push_back(c); break;
        }
    }
    return o;
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 0; }
    std::string cmd = argv[1];
    if (cmd != "qa") { usage(); return 1; }

    std::string host;
    uint16_t port = 5060;
    int timeout_ms = 1200;
    int retries = 2;

    std::string from_uri, to_uri;

    bool do_register = false;
    std::string aor_uri, contact_uri, user, pass;
    int expires = 300;

    // very simple arg parse
    for (int i=2;i<argc;i++){
        std::string a = argv[i];
        auto need = [&](const char* name)->std::string{
            if (i+1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(2); }
            return argv[++i];
        };

        if (a == "--host") host = need("--host");
        else if (a == "--port") port = (uint16_t)std::stoi(need("--port"));
        else if (a == "--timeout") timeout_ms = std::stoi(need("--timeout"));
        else if (a == "--retries") retries = std::stoi(need("--retries"));
        else if (a == "--from") from_uri = need("--from");
        else if (a == "--to") to_uri = need("--to");
        else if (a == "--register") do_register = true;
        else if (a == "--aor") aor_uri = need("--aor");
        else if (a == "--contact") contact_uri = need("--contact");
        else if (a == "--user") user = need("--user");
        else if (a == "--pass") pass = need("--pass");
        else if (a == "--expires") expires = std::stoi(need("--expires"));
        else {
            std::cerr << "Unknown arg: " << a << "\n";
            return 2;
        }
    }

    if (host.empty() || from_uri.empty() || to_uri.empty()) {
        std::cerr << "Missing required args.\n";
        usage();
        return 2;
    }
    if (do_register) {
        if (aor_uri.empty() || contact_uri.empty() || user.empty() || pass.empty()) {
            std::cerr << "--register requires --aor --contact --user --pass\n";
            return 2;
        }
    }

    fs::path data = app_data_dir();
    fs::create_directories(data);
    fs::path report_path = data / "sip_qa_report.json";

    UdpClient udp;
    if (!udp.open()) {
        std::cerr << "Failed to open UDP socket.\n";
        return 3;
    }

    UdpEndpoint ep{host, port};
    std::string ua = "frogklan-sip-qa/" + std::string(APP_VERSION);

    // OPTIONS probe
    SipProbeResult opt_res;
    {
        std::string call_id = rand_hex(12) + "@frogklan";
        std::string branch = "z9hG4bK-" + rand_hex(8);
        std::string tag = rand_hex(6);

        int cseq = 1;
        std::string msg = make_sip_options(host, port, from_uri, to_uri, ua, call_id, cseq, branch, tag);

        UdpReply rep;
        for (int k=0;k<=retries;k++){
            rep = udp.request(ep, msg, timeout_ms);
            if (rep.ok) break;
        }

        if (!rep.ok) {
            opt_res.ok = false;
            opt_res.note = "No reply to OPTIONS (timeout)";
        } else {
            auto resp = parse_sip_response(rep.data);
            opt_res.ok = (resp.status >= 100);
            opt_res.status = resp.status;
            opt_res.rtt_ms = rep.elapsed_ms;
            opt_res.peer_ip = rep.peer_ip;
            opt_res.peer_port = rep.peer_port;
            opt_res.note = resp.reason;
        }
    }

    // REGISTER probe (optional)
    SipProbeResult reg_res;
    if (do_register) {
        std::string call_id = rand_hex(12) + "@frogklan";
        std::string branch = "z9hG4bK-" + rand_hex(8);
        std::string tag = rand_hex(6);
        int cseq = 1;

        // 1) initial REGISTER
        std::string msg1 = make_sip_register(host, port, aor_uri, contact_uri, ua, call_id, cseq, branch, tag, expires, "");
        UdpReply rep1;
        for (int k=0;k<=retries;k++){
            rep1 = udp.request(ep, msg1, timeout_ms);
            if (rep1.ok) break;
        }

        if (!rep1.ok) {
            reg_res.ok = false;
            reg_res.note = "No reply to REGISTER (timeout)";
        } else {
            auto resp1 = parse_sip_response(rep1.data);
            reg_res.status = resp1.status;
            reg_res.rtt_ms = rep1.elapsed_ms;
            reg_res.peer_ip = rep1.peer_ip;
            reg_res.peer_port = rep1.peer_port;
            reg_res.note = resp1.reason;

            if (resp1.status == 401 || resp1.status == 407) {
                auto ch = parse_www_authenticate_digest(resp1);
                if (!ch.ok) {
                    reg_res.ok = false;
                    reg_res.note = "REGISTER got 401 but no parsable Digest challenge";
                } else {
                    // 2) retry with Authorization
                    std::string uri = "sip:" + host + (port != 5060 ? (":" + std::to_string(port)) : "");
                    std::string cnonce = rand_hex(8);
                    std::string nc = "00000001";
                    std::string auth = build_digest_authorization("REGISTER", uri, user, pass, ch, cnonce, nc);

                    cseq += 1;
                    std::string msg2 = make_sip_register(host, port, aor_uri, contact_uri, ua, call_id, cseq,
                                                         "z9hG4bK-" + rand_hex(8), tag, expires, auth);

                    UdpReply rep2;
                    for (int k=0;k<=retries;k++){
                        rep2 = udp.request(ep, msg2, timeout_ms);
                        if (rep2.ok) break;
                    }

                    if (!rep2.ok) {
                        reg_res.ok = false;
                        reg_res.note = "REGISTER auth retry timed out";
                    } else {
                        auto resp2 = parse_sip_response(rep2.data);
                        reg_res.ok = (resp2.status >= 200 && resp2.status < 300);
                        reg_res.status = resp2.status;
                        reg_res.rtt_ms = rep2.elapsed_ms;
                        reg_res.peer_ip = rep2.peer_ip;
                        reg_res.peer_port = rep2.peer_port;
                        reg_res.note = resp2.reason;
                    }
                }
            } else {
                reg_res.ok = (resp1.status >= 200 && resp1.status < 300);
            }
        }
    }

    // Write JSON report
    std::ofstream f(report_path);
    f <<
"{\n"
"  \"app\": \"" << APP_NAME << "\",\n"
"  \"version\": \"" << json_escape(APP_VERSION) << "\",\n"
"  \"os\": \"" << json_escape(os_name()) << "\",\n"
"  \"target\": {\"host\": \"" << json_escape(host) << "\", \"port\": " << port << "},\n"
"  \"options\": {\n"
"    \"ok\": " << (opt_res.ok ? "true" : "false") << ",\n"
"    \"status\": " << opt_res.status << ",\n"
"    \"rtt_ms\": " << opt_res.rtt_ms << ",\n"
"    \"peer_ip\": \"" << json_escape(opt_res.peer_ip) << "\",\n"
"    \"peer_port\": " << opt_res.peer_port << ",\n"
"    \"note\": \"" << json_escape(opt_res.note) << "\"\n"
"  }";
    if (do_register) {
        f <<
",\n"
"  \"register\": {\n"
"    \"ok\": " << (reg_res.ok ? "true" : "false") << ",\n"
"    \"status\": " << reg_res.status << ",\n"
"    \"rtt_ms\": " << reg_res.rtt_ms << ",\n"
"    \"peer_ip\": \"" << json_escape(reg_res.peer_ip) << "\",\n"
"    \"peer_port\": " << reg_res.peer_port << ",\n"
"    \"note\": \"" << json_escape(reg_res.note) << "\"\n"
"  }";
    }
    f << "\n}\n";
    f.close();

    // Console summary
    std::cout << "SIP QA report: " << report_path << "\n";
    std::cout << "OPTIONS: " << (opt_res.ok ? "OK" : "FAIL")
              << " status=" << opt_res.status << " rtt_ms=" << opt_res.rtt_ms
              << " peer=" << opt_res.peer_ip << ":" << opt_res.peer_port
              << " (" << opt_res.note << ")\n";
    if (do_register) {
        std::cout << "REGISTER: " << (reg_res.ok ? "OK" : "FAIL")
                  << " status=" << reg_res.status << " rtt_ms=" << reg_res.rtt_ms
                  << " peer=" << reg_res.peer_ip << ":" << reg_res.peer_port
                  << " (" << reg_res.note << ")\n";
    }

    return 0;
}
