#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 13: Sign APK with apksigner and zipalign
fs::path phase_sign_align(const KineticConfig& cfg,
                           const KineticEnv& env,
                           const fs::path& project_root,
                           const fs::path& apk_path);
} // namespace kinetic
