#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  copy/file_copier.hpp  —  Validated file copy with SHA-256 verification
// ─────────────────────────────────────────────────────────────────────────────
#include <filesystem>
#include <string>
#include <vector>

namespace kinetic {

namespace fs = std::filesystem;

// Copy src → dst.
// If verbose is true, prints a log line per file.
// If validate_sha is true, verifies SHA-256 pre/post copy.
// Throws (via kinetic::fatal) on any failure.
void copy_file_validated(const fs::path& src,
                         const fs::path& dst,
                         bool verbose      = true,
                         bool validate_sha = true,
                         const std::string& phase = "ASSET_COPY");

// Copy all files in src_dir recursively into dst_dir.
// Returns count of files copied.
int copy_dir_recursive(const fs::path& src_dir,
                       const fs::path& dst_dir,
                       bool verbose = true,
                       const std::string& phase = "ASSET_COPY");

} // namespace kinetic
