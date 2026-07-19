// ─────────────────────────────────────────────────────────────────────────────
//  bridge.cpp  —  The single C-callable entry point that drives the C++ engine.
//
//  `main.c` calls kinetic_engine_run(); here we instantiate the CLI parser
//  and the BuildEngine and dispatch.  All C++ exceptions are caught here so
//  they never cross the C boundary.
// ─────────────────────────────────────────────────────────────────────────────
#include "kinetic.h"

#include "kinetic.hpp"
#include "cli/cli_parser.hpp"
#include "core/engine.hpp"

#include <filesystem>
#include <iostream>
#include <new>

namespace fs = std::filesystem;

extern "C"
int kinetic_engine_run(int argc, char **argv, const char *cwd)
{
    try {
        kinetic::CliOptions opts = kinetic::parse_cli(argc, argv);

        fs::path project_root = (cwd != NULL && cwd[0] != '\0')
                                  ? fs::path(cwd)
                                  : fs::current_path();

        // Sync the global C++ color flag (kinetic::g_color_enabled) down to
        // the C flag (kinetic_color_enabled).  Match the previous main.cpp
        // behaviour where this happens inside the engine constructor; we do
        // it explicitly here for the C++ surface.
        if (opts.no_color) {
            g_color_enabled = false;
        }

        kinetic::BuildEngine engine(opts, project_root);
        int rc = engine.run();
        return rc;
    } catch (const std::runtime_error&) {
        // Fatal errors already printed by kinetic::fatal() — just exit.
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mFatal: \033[0m" << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\033[1;31mFatal: \033[0munknown error\n";
        return 1;
    }
}
