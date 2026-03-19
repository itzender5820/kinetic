#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
#include <vector>
#include <string>
namespace kinetic {
namespace fs = std::filesystem;
// Compile all C++ sources listed in cfg.sources for each ABI.
// Returns paths to the produced .so files.
std::vector<fs::path> phase_ndk_compile(const KineticConfig& cfg,
                                         const KineticEnv& env,
                                         const fs::path& project_root,
                                         const fs::path& obj_dir,
                                         const fs::path& lib_dir);
} // namespace kinetic
