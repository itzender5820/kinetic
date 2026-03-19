// ─────────────────────────────────────────────────────────────────────────────
//  utils/sha256.cpp  —  Portable SHA-256 implementation (FIPS 180-4)
// ─────────────────────────────────────────────────────────────────────────────
#include "sha256.hpp"
#include <fstream>
#include <array>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace kinetic {

// ── SHA-256 internals ─────────────────────────────────────────────────────────
static constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t Ch (uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t S0 (uint32_t x) { return rotr(x,2)  ^ rotr(x,13) ^ rotr(x,22); }
static inline uint32_t S1 (uint32_t x) { return rotr(x,6)  ^ rotr(x,11) ^ rotr(x,25); }
static inline uint32_t s0 (uint32_t x) { return rotr(x,7)  ^ rotr(x,18) ^ (x >> 3);  }
static inline uint32_t s1 (uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

struct SHA256State {
    uint32_t h[8];
    uint8_t  buf[64];
    uint64_t total_bits;
    uint32_t buf_len;

    SHA256State() : total_bits(0), buf_len(0) {
        h[0] = 0x6a09e667; h[1] = 0xbb67ae85;
        h[2] = 0x3c6ef372; h[3] = 0xa54ff53a;
        h[4] = 0x510e527f; h[5] = 0x9b05688c;
        h[6] = 0x1f83d9ab; h[7] = 0x5be0cd19;
    }

    void process_block(const uint8_t block[64]) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)block[i*4]   << 24) |
                   ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8)  |
                   ((uint32_t)block[i*4+3]);
        }
        for (int i = 16; i < 64; ++i)
            w[i] = s1(w[i-2]) + w[i-7] + s0(w[i-15]) + w[i-16];

        uint32_t a=h[0], b=h[1], c=h[2], d=h[3],
                 e=h[4], f=h[5], g=h[6], hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = hh + S1(e) + Ch(e,f,g) + K[i] + w[i];
            uint32_t t2 = S0(a) + Maj(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    void update(const uint8_t* data, size_t len) {
        total_bits += (uint64_t)len * 8;
        while (len > 0) {
            size_t space = 64 - buf_len;
            size_t copy  = len < space ? len : space;
            std::memcpy(buf + buf_len, data, copy);
            buf_len += (uint32_t)copy;
            data    += copy;
            len     -= copy;
            if (buf_len == 64) {
                process_block(buf);
                buf_len = 0;
            }
        }
    }

    std::array<uint8_t, 32> finalise() {
        // Padding
        buf[buf_len++] = 0x80;
        if (buf_len > 56) {
            while (buf_len < 64) buf[buf_len++] = 0;
            process_block(buf);
            buf_len = 0;
        }
        while (buf_len < 56) buf[buf_len++] = 0;
        for (int i = 7; i >= 0; --i)
            buf[buf_len++] = (uint8_t)(total_bits >> (i * 8));
        process_block(buf);

        std::array<uint8_t, 32> digest;
        for (int i = 0; i < 8; ++i) {
            digest[i*4+0] = (uint8_t)(h[i] >> 24);
            digest[i*4+1] = (uint8_t)(h[i] >> 16);
            digest[i*4+2] = (uint8_t)(h[i] >> 8);
            digest[i*4+3] = (uint8_t)(h[i]);
        }
        return digest;
    }
};

static std::string to_hex(const std::array<uint8_t,32>& d) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : d) ss << std::setw(2) << (int)b;
    return ss.str();
}

// ── Public API ─────────────────────────────────────────────────────────────────
std::string sha256_data(const uint8_t* data, size_t len) {
    SHA256State st;
    st.update(data, len);
    return to_hex(st.finalise());
}

std::string sha256_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    SHA256State st;
    uint8_t buf[8192];
    while (f) {
        f.read(reinterpret_cast<char*>(buf), sizeof(buf));
        std::streamsize n = f.gcount();
        if (n > 0) st.update(buf, (size_t)n);
    }
    return to_hex(st.finalise());
}

} // namespace kinetic
