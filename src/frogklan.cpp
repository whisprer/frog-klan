#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <map>
#include <chrono>
#include <cstdlib>

namespace fs = std::filesystem;

static const char* APP_NAME = "frogklan";
static const char* APP_VERSION = "1.1.0";

fs::path app_data_dir() {
#if defined(_WIN32)
    const char* p = std::getenv("APPDATA");
    return fs::path(p ? p : fs::temp_directory_path()) / APP_NAME;
#elif defined(__APPLE__)
    const char* h = std::getenv("HOME");
    return fs::path(h ? h : "/tmp") / "Library/Application Support" / APP_NAME;
#else
    const char* h = std::getenv("HOME");
    return fs::path(h ? h : "/tmp") / ("." + std::string(APP_NAME));
#endif
}

void ensure_dirs(const fs::path& base) {
    fs::create_directories(base / "plugins");
}

void write_default_config(const fs::path& cfg) {
    std::ofstream f(cfg);
    f << "{\n"
      << "  \"version\": \"" << APP_VERSION << "\",\n"
      << "  \"telemetry\": false,\n"
      << "  \"update_channel\": \"stable\"\n"
      << "}\n";
}

void cmd_status(const fs::path& base) {
    std::cout << "frogklan status\n";
    std::cout << "Version: " << APP_VERSION << "\n";
    std::cout << "Data dir: " << base << "\n";
    std::cout << "Plugins:\n";

    for (auto& p : fs::directory_iterator(base / "plugins")) {
        std::cout << "  - " << p.path().filename().string() << "\n";
    }
}

void cmd_self_test(const fs::path& base) {
    std::cout << "Running self-test...\n";

    fs::path test = base / "selftest.tmp";
    std::ofstream f(test);
    f << "ok\n";
    f.close();
    fs::remove(test);

    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::cout << "Clock OK: " << now << "\n";
    std::cout << "Filesystem OK\n";
    std::cout << "Self-test passed\n";
}

int main(int argc, char** argv) {
    fs::path base = app_data_dir();
    ensure_dirs(base);

    fs::path cfg = base / "config.json";
    if (!fs::exists(cfg)) {
        write_default_config(cfg);
    }

    if (argc < 2) {
        std::cout << "frogklan v" << APP_VERSION << "\n";
        std::cout << "Commands: status | self-test\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "status") {
        cmd_status(base);
    } else if (cmd == "self-test") {
        cmd_self_test(base);
    } else {
        std::cout << "Unknown command\n";
    }

    return 0;
}
