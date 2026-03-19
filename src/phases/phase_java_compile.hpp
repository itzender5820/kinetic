#pragma once
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include <filesystem>
namespace kinetic {
namespace fs = std::filesystem;
// Phase 06: Compile Java sources → .class files. Returns class output dir.
fs::path phase_java_compile(const KineticConfig& cfg,
                             const KineticEnv& env,
                             const fs::path& project_root,
                             const fs::path& classes_dir);
} // namespace kinetic
