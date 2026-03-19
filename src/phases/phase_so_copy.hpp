#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
#include <vector>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 09: Copy compiled .so files into staging lib/<abi>/ directories
int phase_so_copy(const KineticConfig& cfg,
                  const KineticEnv& env,
                  const fs::path& project_root,
                  const fs::path& lib_dir,
                  const fs::path& staging_dir);
} // namespace kinetic
