#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  utils/sha256.hpp  —  SHA-256 hash computation (no OpenSSL dependency)
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <filesystem>
#include <cstdint>

namespace kinetic {

namespace fs = std::filesystem;

// Compute the SHA-256 digest of the file at the given path.
// Returns lowercase hex string (64 chars), or empty string on I/O error.
std::string sha256_file(const fs::path& path);

// Compute the SHA-256 digest of a block of memory.
std::string sha256_data(const uint8_t* data, size_t len);

} // namespace kinetic
