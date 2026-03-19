#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  core/engine.hpp  —  Pipeline orchestration
// ─────────────────────────────────────────────────────────────────────────────
#include "../cli/cli_parser.hpp"
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include "phase_timer.hpp"
#include <filesystem>

namespace kinetic {

namespace fs = std::filesystem;

class BuildEngine {
public:
    BuildEngine(const CliOptions& opts, const fs::path& project_root);

    // Run the command specified in opts
    int run();

private:
    CliOptions  opts_;
    fs::path    project_root_;
    KineticEnv  env_;
    KineticConfig cfg_;
    PhaseTimer  timer_;

    // Build directory layout
    fs::path build_dir_;
    fs::path flat_dir_;
    fs::path aprs_dir_;
    fs::path obj_dir_;
    fs::path lib_dir_;
    fs::path classes_dir_;
    fs::path dex_dir_;
    fs::path staging_dir_;
    fs::path output_dir_;

    void setup_dirs();

    int cmd_build();
    int cmd_clean();
    int cmd_rebuild();
    int cmd_install();
    int cmd_run();
    int cmd_check();
    int cmd_env();
    int cmd_version();

    // Run ADB command; returns exit code
    int adb(const std::vector<std::string>& args);

    // Log a separator line
    void separator() const;
};

} // namespace kinetic
