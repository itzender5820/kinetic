// ─────────────────────────────────────────────────────────────────────────────
//  copy/file_copier.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/sha256.hpp"
#include "../kinetic.hpp"

#include <stdexcept>

namespace kinetic {

void copy_file_validated(const fs::path& src,
                         const fs::path& dst,
                         bool verbose,
                         bool validate_sha,
                         const std::string& phase) {
    if (!fs::exists(src)) {
        fatal(phase, err::CPY_001,
              "Source file missing: " + src.filename().string(),
              src.string());
    }

    // Compute source hash before copy
    std::string pre_hash;
    if (validate_sha) {
        pre_hash = sha256_file(src);
    }

    // Ensure destination directory exists
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec) {
        fatal(phase, err::CPY_002,
              "Cannot create destination directory",
              dst.parent_path().string(),
              "Error: " + ec.message());
    }

    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        fatal(phase, err::CPY_002,
              "File copy failed: " + src.filename().string(),
              src.string(),
              "Destination: " + dst.string() + "\nError: " + ec.message());
    }

    // Verify hash after copy
    if (validate_sha && !pre_hash.empty()) {
        std::string post_hash = sha256_file(dst);
        if (post_hash != pre_hash) {
            fatal(phase, err::CPY_003,
                  "SHA-256 mismatch after copy: " + dst.filename().string(),
                  dst.string(),
                  "Expected: " + pre_hash + "\nGot:      " + post_hash);
        }
    }

    if (verbose) {
        phase_log(phase, src.filename().string() + "  →  " + dst.parent_path().filename().string() + "/  ✓");
    }
}

int copy_dir_recursive(const fs::path& src_dir,
                       const fs::path& dst_dir,
                       bool verbose,
                       const std::string& phase) {
    if (!fs::exists(src_dir)) {
        return 0; // Not an error — caller decides
    }

    int count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) continue;

        fs::path rel  = fs::relative(entry.path(), src_dir);
        fs::path dest = dst_dir / rel;

        copy_file_validated(entry.path(), dest, verbose, true, phase);
        ++count;
    }
    return count;
}

} // namespace kinetic
