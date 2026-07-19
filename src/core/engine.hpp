// ─────────────────────────────────────────────────────────────────────────────
//  core/engine.hpp  —  DAG-driven pipeline orchestration
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../cli/cli_parser.hpp"
#include "../dag/dag.hpp"
#include "../dag/executor.hpp"
#include "../dag/phase.hpp"
#include "../env/env_scanner.hpp"
#include "../config/config_model.hpp"
#include "phase_timer.hpp"
#include <filesystem>

namespace kinetic {

namespace fs = std::filesystem;

class BuildEngine {
public:
    BuildEngine(const CliOptions& opts, const fs::path& project_root);

    int run();

private:
    CliOptions  opts_;
    fs::path    project_root_;

    // Command handlers
    int cmd_build();
    int cmd_clean();
    int cmd_rebuild();
    int cmd_install();
    int cmd_run();
    int cmd_check();
    int cmd_env();
    int cmd_version();

    int adb(const std::vector<std::string>& args);

    void separator() const;
};

} // namespace kinetic
