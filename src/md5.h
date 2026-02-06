#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MD5 {
    MD5();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& s);
    std::string final_hex();

    static std::string hex(const uint8_t* d, size_t n);
    static std::string md5_hex(const std::string& s);

private:
    void transform(const uint8_t block[64]);
    void finalize(uint8_t out[16]);

    uint32_t a_, b_, c_, d_;
    uint64_t bytes_;
    uint8_t buffer_[64];
    size_t buffer_len_;
    bool finalized_;
};
