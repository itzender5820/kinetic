#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
fs::path phase_kotlin_compile(const KineticConfig& cfg,
                               const KineticEnv& env,
                               const fs::path& project_root,
                               const fs::path& classes_dir);
} // namespace kinetic
