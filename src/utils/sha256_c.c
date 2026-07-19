/*
 * ============================================================================
 *  utils/sha256_c.c  —  Portable SHA-256 implementation (FIPS 180-4)
 *
 *  Rewritten from the C++ src/utils/sha256.cpp in portable C99.  No I/O
 *  uses C++ FILE-stream — we still use <stdio.h> for the file variant for
 *  portability, since <stdio.h> is part of the standard C library.
 * ============================================================================
 */
#include "kinetic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ────────────────────────────────────────────────────────────────────────
 *  Round constants and pre-computed message schedule seeds.
 * ────────────────────────────────────────────────────────────────────── */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static inline uint32_t rotr(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}
static inline uint32_t Ch (uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (~x & z); }
static inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t BSig0(uint32_t x){ return rotr(x, 2)  ^ rotr(x, 13) ^ rotr(x, 22); }
static inline uint32_t BSig1(uint32_t x){ return rotr(x, 6)  ^ rotr(x, 11) ^ rotr(x, 25); }
static inline uint32_t SSig0(uint32_t x){ return rotr(x, 7)  ^ rotr(x,18) ^ (x >> 3);  }
static inline uint32_t SSig1(uint32_t x){ return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

typedef struct {
    uint32_t h[8];
    uint8_t  buf[64];
    uint64_t total_bits;
    uint32_t buf_len;
} sha256_ctx;

static void sha256_init(sha256_ctx *st)
{
    st->h[0] = 0x6a09e667u; st->h[1] = 0xbb67ae85u;
    st->h[2] = 0x3c6ef372u; st->h[3] = 0xa54ff53au;
    st->h[4] = 0x510e527fu; st->h[5] = 0x9b05688cu;
    st->h[6] = 0x1f83d9abu; st->h[7] = 0x5be0cd19u;
    st->total_bits = 0;
    st->buf_len    = 0;
}

static void sha256_process_block(sha256_ctx *st, const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[i*4]   << 24) |
               ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] <<  8) |
               ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = SSig1(w[i-2]) + w[i-7] + SSig0(w[i-15]) + w[i-16];
    }

    uint32_t a = st->h[0], b = st->h[1], c = st->h[2], d = st->h[3];
    uint32_t e = st->h[4], f = st->h[5], g = st->h[6], h = st->h[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + BSig1(e) + Ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = BSig0(a) + Maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    st->h[0] += a; st->h[1] += b; st->h[2] += c; st->h[3] += d;
    st->h[4] += e; st->h[5] += f; st->h[6] += g; st->h[7] += h;
}

static void sha256_update(sha256_ctx *st, const uint8_t *data, size_t len)
{
    st->total_bits += (uint64_t)len * 8u;
    while (len > 0) {
        size_t space = 64u - st->buf_len;
        size_t copy  = len < space ? len : space;
        memcpy(st->buf + st->buf_len, data, copy);
        st->buf_len += (uint32_t)copy;
        data        += copy;
        len         -= copy;
        if (st->buf_len == 64u) {
            sha256_process_block(st, st->buf);
            st->buf_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx *st, uint8_t out[32])
{
    /* Append 0x80 and pad with zeros up to 56 bytes, leaving room for the
     * 8-byte length field.  If buf_len is already > 56, pad into a new
     * block first. */
    st->buf[st->buf_len++] = 0x80u;
    if (st->buf_len > 56u) {
        while (st->buf_len < 64u) st->buf[st->buf_len++] = 0u;
        sha256_process_block(st, st->buf);
        st->buf_len = 0;
    }
    while (st->buf_len < 56u) st->buf[st->buf_len++] = 0u;

    /* Append total bit length, big-endian */
    for (int i = 7; i >= 0; --i) {
        st->buf[st->buf_len++] = (uint8_t)(st->total_bits >> (i * 8));
    }
    sha256_process_block(st, st->buf);

    for (int i = 0; i < 8; ++i) {
        out[i*4+0] = (uint8_t)(st->h[i] >> 24);
        out[i*4+1] = (uint8_t)(st->h[i] >> 16);
        out[i*4+2] = (uint8_t)(st->h[i] >>  8);
        out[i*4+3] = (uint8_t)(st->h[i]);
    }
}

static void sha256_to_hex(const uint8_t digest[32], char out[65])
{
    static const char *hexd = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i*2+0] = hexd[digest[i] >> 4];
        out[i*2+1] = hexd[digest[i] & 0x0f];
    }
    out[64] = '\0';
}

/* ────────────────────────────────────────────────────────────────────────
 *  Public API
 * ────────────────────────────────────────────────────────────────────── */
void kinetic_sha256_data(const uint8_t *data, size_t len, char out[65])
{
    sha256_ctx st;
    sha256_init(&st);
    sha256_update(&st, data, len);
    uint8_t digest[32];
    sha256_final(&st, digest);
    sha256_to_hex(digest, out);
}

int kinetic_sha256_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;

    sha256_ctx st;
    sha256_init(&st);

    uint8_t buf[8192];
    size_t  n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        sha256_update(&st, buf, n);

    int read_err = ferror(f);
    fclose(f);
    if (read_err) return -1;

    uint8_t digest[32];
    sha256_final(&st, digest);
    sha256_to_hex(digest, out);
    return 0;
}
