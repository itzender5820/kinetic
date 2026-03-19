#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  copy/essential_manifest.hpp  —  All Gradle-silent essential files
// ─────────────────────────────────────────────────────────────────────────────
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
#include <vector>
#include <string>

namespace kinetic {

namespace fs = std::filesystem;

struct EssentialFile {
    std::string category;   // e.g. "libc++_shared.so"
    fs::path    src;        // absolute source path
    fs::path    dst;        // absolute destination path (inside staging)
    bool        required;   // fatal if missing when true
};

// Build the list of essential files that Gradle copies silently.
// staging_dir: the APK staging directory (e.g. build/intermediates/staging/)
std::vector<EssentialFile> build_essential_manifest(
    const KineticConfig& cfg,
    const KineticEnv& env,
    const fs::path& staging_dir,
    const fs::path& project_root);

// Copy all essential files, logging each one.
// Returns number of files successfully copied.
int copy_essential_files(const std::vector<EssentialFile>& manifest,
                         bool verbose_copy);

} // namespace kinetic
