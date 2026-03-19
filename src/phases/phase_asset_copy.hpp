#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 10: Copy assets/, res/raw/, essential silent files, extra assets
int phase_asset_copy(const KineticConfig& cfg,
                     const KineticEnv& env,
                     const fs::path& project_root,
                     const fs::path& staging_dir);
} // namespace kinetic
