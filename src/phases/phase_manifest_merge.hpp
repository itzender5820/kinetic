#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 11: Validate and copy AndroidManifest.xml into staging
fs::path phase_manifest_merge(const KineticConfig& cfg,
                               const KineticEnv& env,
                               const fs::path& project_root,
                               const fs::path& staging_dir);
} // namespace kinetic
