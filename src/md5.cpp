#include "md5.h"
#include <cstring>

static inline uint32_t rol(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }
static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

MD5::MD5()
: a_(0x67452301u), b_(0xefcdab89u), c_(0x98badcfeu), d_(0x10325476u),
  bytes_(0), buffer_len_(0), finalized_(false) {
    std::memset(buffer_, 0, sizeof(buffer_));
}

void MD5::update(const std::string& s) { update(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

void MD5::update(const uint8_t* data, size_t len) {
    if (finalized_) return;
    bytes_ += len;

    size_t i = 0;
    if (buffer_len_ > 0) {
        size_t take = (len < (64 - buffer_len_)) ? len : (64 - buffer_len_);
        std::memcpy(buffer_ + buffer_len_, data, take);
        buffer_len_ += take;
        i += take;
        if (buffer_len_ == 64) {
            transform(buffer_);
            buffer_len_ = 0;
        }
    }
    for (; i + 64 <= len; i += 64) transform(data + i);

    size_t rem = len - i;
    if (rem > 0) {
        std::memcpy(buffer_, data + i, rem);
        buffer_len_ = rem;
    }
}

void MD5::transform(const uint8_t block[64]) {
    uint32_t X[16];
    for (int i = 0; i < 16; i++) {
        X[i] = (uint32_t)block[i*4] |
               ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) |
               ((uint32_t)block[i*4+3] << 24);
    }

    uint32_t a=a_, b=b_, c=c_, d=d_;

#define STEP(f,a,b,c,d,x,t,s) a = b + rol(a + f(b,c,d) + x + t, s)

    // Round 1
    STEP(F,a,b,c,d,X[ 0],0xd76aa478, 7);
    STEP(F,d,a,b,c,X[ 1],0xe8c7b756,12);
    STEP(F,c,d,a,b,X[ 2],0x242070db,17);
    STEP(F,b,c,d,a,X[ 3],0xc1bdceee,22);
    STEP(F,a,b,c,d,X[ 4],0xf57c0faf, 7);
    STEP(F,d,a,b,c,X[ 5],0x4787c62a,12);
    STEP(F,c,d,a,b,X[ 6],0xa8304613,17);
    STEP(F,b,c,d,a,X[ 7],0xfd469501,22);
    STEP(F,a,b,c,d,X[ 8],0x698098d8, 7);
    STEP(F,d,a,b,c,X[ 9],0x8b44f7af,12);
    STEP(F,c,d,a,b,X[10],0xffff5bb1,17);
    STEP(F,b,c,d,a,X[11],0x895cd7be,22);
    STEP(F,a,b,c,d,X[12],0x6b901122, 7);
    STEP(F,d,a,b,c,X[13],0xfd987193,12);
    STEP(F,c,d,a,b,X[14],0xa679438e,17);
    STEP(F,b,c,d,a,X[15],0x49b40821,22);

    // Round 2
    STEP(G,a,b,c,d,X[ 1],0xf61e2562, 5);
    STEP(G,d,a,b,c,X[ 6],0xc040b340, 9);
    STEP(G,c,d,a,b,X[11],0x265e5a51,14);
    STEP(G,b,c,d,a,X[ 0],0xe9b6c7aa,20);
    STEP(G,a,b,c,d,X[ 5],0xd62f105d, 5);
    STEP(G,d,a,b,c,X[10],0x02441453, 9);
    STEP(G,c,d,a,b,X[15],0xd8a1e681,14);
    STEP(G,b,c,d,a,X[ 4],0xe7d3fbc8,20);
    STEP(G,a,b,c,d,X[ 9],0x21e1cde6, 5);
    STEP(G,d,a,b,c,X[14],0xc33707d6, 9);
    STEP(G,c,d,a,b,X[ 3],0xf4d50d87,14);
    STEP(G,b,c,d,a,X[ 8],0x455a14ed,20);
    STEP(G,a,b,c,d,X[13],0xa9e3e905, 5);
    STEP(G,d,a,b,c,X[ 2],0xfcefa3f8, 9);
    STEP(G,c,d,a,b,X[ 7],0x676f02d9,14);
    STEP(G,b,c,d,a,X[12],0x8d2a4c8a,20);

    // Round 3
    STEP(H,a,b,c,d,X[ 5],0xfffa3942, 4);
    STEP(H,d,a,b,c,X[ 8],0x8771f681,11);
    STEP(H,c,d,a,b,X[11],0x6d9d6122,16);
    STEP(H,b,c,d,a,X[14],0xfde5380c,23);
    STEP(H,a,b,c,d,X[ 1],0xa4beea44, 4);
    STEP(H,d,a,b,c,X[ 4],0x4bdecfa9,11);
    STEP(H,c,d,a,b,X[ 7],0xf6bb4b60,16);
    STEP(H,b,c,d,a,X[10],0xbebfbc70,23);
    STEP(H,a,b,c,d,X[13],0x289b7ec6, 4);
    STEP(H,d,a,b,c,X[ 0],0xeaa127fa,11);
    STEP(H,c,d,a,b,X[ 3],0xd4ef3085,16);
    STEP(H,b,c,d,a,X[ 6],0x04881d05,23);
    STEP(H,a,b,c,d,X[ 9],0xd9d4d039, 4);
    STEP(H,d,a,b,c,X[12],0xe6db99e5,11);
    STEP(H,c,d,a,b,X[15],0x1fa27cf8,16);
    STEP(H,b,c,d,a,X[ 2],0xc4ac5665,23);

    // Round 4
    STEP(I,a,b,c,d,X[ 0],0xf4292244, 6);
    STEP(I,d,a,b,c,X[ 7],0x432aff97,10);
    STEP(I,c,d,a,b,X[14],0xab9423a7,15);
    STEP(I,b,c,d,a,X[ 5],0xfc93a039,21);
    STEP(I,a,b,c,d,X[12],0x655b59c3, 6);
    STEP(I,d,a,b,c,X[ 3],0x8f0ccc92,10);
    STEP(I,c,d,a,b,X[10],0xffeff47d,15);
    STEP(I,b,c,d,a,X[ 1],0x85845dd1,21);
    STEP(I,a,b,c,d,X[ 8],0x6fa87e4f, 6);
    STEP(I,d,a,b,c,X[15],0xfe2ce6e0,10);
    STEP(I,c,d,a,b,X[ 6],0xa3014314,15);
    STEP(I,b,c,d,a,X[13],0x4e0811a1,21);
    STEP(I,a,b,c,d,X[ 4],0xf7537e82, 6);
    STEP(I,d,a,b,c,X[11],0xbd3af235,10);
    STEP(I,c,d,a,b,X[ 2],0x2ad7d2bb,15);
    STEP(I,b,c,d,a,X[ 9],0xeb86d391,21);

#undef STEP

    a_ += a; b_ += b; c_ += c; d_ += d;
}

void MD5::finalize(uint8_t out[16]) {
    uint64_t bits = bytes_ * 8;
    uint8_t pad[64] = {0x80};

    size_t pad_len = (buffer_len_ < 56) ? (56 - buffer_len_) : (56 + (64 - buffer_len_));
    update(pad, pad_len);

    uint8_t len8[8];
    for (int i=0;i<8;i++) len8[i] = (uint8_t)((bits >> (8*i)) & 0xff);
    update(len8, 8);

    auto write32 = [&](uint32_t v, int idx){
        out[idx+0] = (uint8_t)(v & 0xff);
        out[idx+1] = (uint8_t)((v >> 8) & 0xff);
        out[idx+2] = (uint8_t)((v >> 16) & 0xff);
        out[idx+3] = (uint8_t)((v >> 24) & 0xff);
    };
    write32(a_,0); write32(b_,4); write32(c_,8); write32(d_,12);
    finalized_ = true;
}

std::string MD5::hex(const uint8_t* d, size_t n) {
    static const char* h="0123456789abcdef";
    std::string s; s.reserve(n*2);
    for (size_t i=0;i<n;i++){ s.push_back(h[d[i]>>4]); s.push_back(h[d[i]&15]); }
    return s;
}

std::string MD5::final_hex() {
    uint8_t out[16];
    if (!finalized_) finalize(out);
    else {
        // If already finalized, recompute not supported; caller shouldn't re-finalize.
        // Return empty to avoid implying safety.
        return "";
    }
    return hex(out,16);
}

std::string MD5::md5_hex(const std::string& s) {
    MD5 m; m.update(s); return m.final_hex();
}
