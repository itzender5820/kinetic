#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Link .flat files into resources.aprs. Returns the path to the produced .aprs.
fs::path phase_link_aprs(const KineticConfig& cfg,
                          const KineticEnv& env,
                          const fs::path& project_root,
                          const fs::path& flat_dir,
                          const fs::path& aprs_dir);
} // namespace kinetic
