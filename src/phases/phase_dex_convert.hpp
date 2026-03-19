#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 08: Convert .class files to .dex using d8
fs::path phase_dex_convert(const KineticConfig& cfg,
                            const KineticEnv& env,
                            const fs::path& project_root,
                            const fs::path& classes_dir,
                            const fs::path& dex_dir);
} // namespace kinetic
