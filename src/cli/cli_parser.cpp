// ─────────────────────────────────────────────────────────────────────────────
//  cli/cli_parser.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "cli_parser.hpp"
#include "help_printer.hpp"
#include "../kinetic.hpp"
#include <stdexcept>
#include <iostream>
#include <cstdlib>

namespace kinetic {

CliOptions parse_cli(int argc, char** argv) {
    CliOptions opts;

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

    if (args.empty()) {
        // Default: full build
        opts.command = Command::Build;
        return opts;
    }

    // First positional-like arg may be a command
    bool command_set = false;
    size_t i = 0;
    while (i < args.size()) {
        const std::string& a = args[i];

        // ── Commands ──────────────────────────────────────────────────────────
        if (!command_set) {
            if (a == "--build")   { opts.command = Command::Build;   command_set = true; ++i; continue; }
            if (a == "--clean")   { opts.command = Command::Clean;   command_set = true; ++i; continue; }
            if (a == "--rebuild") { opts.command = Command::Rebuild; command_set = true; ++i; continue; }
            if (a == "--install") { opts.command = Command::Install; command_set = true; ++i; continue; }
            if (a == "--run")     { opts.command = Command::Run;     command_set = true; ++i; continue; }
            if (a == "--check")   { opts.command = Command::Check;   command_set = true; ++i; continue; }
            if (a == "--env")     { opts.command = Command::Env;     command_set = true; ++i; continue; }
            if (a == "--version") { opts.command = Command::Version; command_set = true; ++i; continue; }
            if (a == "--help" || a == "-h") {
                print_help();
                std::exit(0);
            }
        }

        // ── Build options ─────────────────────────────────────────────────────
        if (a == "--release")  { opts.release_mode = true; ++i; continue; }
        if (a == "--debug")    { opts.release_mode = false; ++i; continue; }
        if (a == "--no-dex")   { opts.no_dex  = true; ++i; continue; }
        if (a == "--no-sign")  { opts.no_sign = true; ++i; continue; }

        if (a == "--abi") {
            if (i + 1 >= args.size()) throw std::runtime_error("--abi requires a value");
            opts.abi_filter = args[++i]; ++i; continue;
        }
        if (a == "--jobs") {
            if (i + 1 >= args.size()) throw std::runtime_error("--jobs requires a value");
            opts.jobs = std::stoi(args[++i]); ++i; continue;
        }

        // ── Output & logging ──────────────────────────────────────────────────
        if (a == "--verbose")  { opts.verbose  = true; ++i; continue; }
        if (a == "--quiet")    { opts.quiet    = true; ++i; continue; }
        if (a == "--no-color") { opts.no_color = true; ++i; continue; }

        if (a == "--log") {
            if (i + 1 >= args.size()) throw std::runtime_error("--log requires a filename");
            opts.log_file = args[++i]; ++i; continue;
        }
        if (a == "--phase") {
            if (i + 1 >= args.size()) throw std::runtime_error("--phase requires a value");
            opts.single_phase = args[++i]; ++i; continue;
        }

        // ── Environment overrides ─────────────────────────────────────────────
        if (a == "--sdk") {
            if (i + 1 >= args.size()) throw std::runtime_error("--sdk requires a path");
            opts.sdk_override = args[++i]; ++i; continue;
        }
        if (a == "--ndk") {
            if (i + 1 >= args.size()) throw std::runtime_error("--ndk requires a path");
            opts.ndk_override = args[++i]; ++i; continue;
        }
        if (a == "--aapt2") {
            if (i + 1 >= args.size()) throw std::runtime_error("--aapt2 requires a path");
            opts.aapt2_override = args[++i]; ++i; continue;
        }

        // Unknown flag
        std::cerr << c(col::YELLOW, "Warning: unknown option: ") << a << "\n";
        ++i;
    }

    return opts;
}

} // namespace kinetic
