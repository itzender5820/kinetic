// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  —  Kinetic entry point, CLI arg parsing, engine dispatch
// ─────────────────────────────────────────────────────────────────────────────
#include "kinetic.hpp"
#include "cli/cli_parser.hpp"
#include "core/engine.hpp"

#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        kinetic::CliOptions opts = kinetic::parse_cli(argc, argv);

        // Project root is always the current working directory
        fs::path project_root = fs::current_path();

        kinetic::BuildEngine engine(opts, project_root);
        return engine.run();

    } catch (const std::runtime_error& e) {
        // Fatal errors from kinetic::fatal() reach here after printing details
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mFatal: \033[0m" << e.what() << "\n";
        return 1;
    }
}
