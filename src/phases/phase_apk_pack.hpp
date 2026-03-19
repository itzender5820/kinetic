#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 12: Package the final APK using the aarch64 system AAPT2
fs::path phase_apk_pack(const KineticConfig& cfg,
                         const KineticEnv& env,
                         const fs::path& project_root,
                         const fs::path& staging_dir,
                         const fs::path& aprs_path,
                         const fs::path& dex_dir,
                         const fs::path& output_dir);
} // namespace kinetic
