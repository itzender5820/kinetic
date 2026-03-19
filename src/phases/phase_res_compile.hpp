#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Returns number of files compiled. Throws on failure.
int phase_res_compile(const KineticConfig& cfg,
                      const KineticEnv& env,
                      const fs::path& project_root,
                      const fs::path& flat_dir);
} // namespace kinetic
