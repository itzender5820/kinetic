#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  cli/cli_parser.hpp  —  Command-line argument parsing
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <optional>

namespace kinetic {

enum class Command {
    Build,
    Clean,
    Rebuild,
    Install,
    Run,
    Check,
    Env,
    Version,
    Help,
};

struct CliOptions {
    Command     command      = Command::Build;

    bool        release_mode = false;
    std::string abi_filter;         // empty = all from kinetic.cmake
    int         jobs         = 0;   // 0 = auto
    bool        no_dex       = false;
    bool        no_sign      = false;

    bool        verbose      = false;
    bool        quiet        = false;
    bool        no_color     = false;
    std::string log_file;
    std::string single_phase; // --phase <name>

    std::string sdk_override;
    std::string ndk_override;
    std::string aapt2_override;

    std::string cmake_file   = "kinetic.cmake";
};

// Parse argc/argv into CliOptions.
// On --help or bad usage, throws or exits.
CliOptions parse_cli(int argc, char** argv);

} // namespace kinetic
