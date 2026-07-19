// ─────────────────────────────────────────────────────────────────────────────
//  core/engine.cpp  —  DAG-driven pipeline orchestration
// ─────────────────────────────────────────────────────────────────────────────
#include "engine.hpp"
#include "phase_registry.hpp"
#include "telemetry.hpp"
#include "../config/cmake_parser.hpp"
#include "../error/error_reporter.hpp"
#include "../utils/process.hpp"
#include "../cli/help_printer.hpp"
#include "../kinetic.hpp"

#include <iostream>

namespace kinetic {

// ── Constructor ───────────────────────────────────────────────────────────────
BuildEngine::BuildEngine(const CliOptions& opts, const fs::path& project_root)
    : opts_(opts), project_root_(project_root) {}

// ── Main entry ────────────────────────────────────────────────────────────────
int BuildEngine::run() {
    if (opts_.no_color) g_color_enabled = false;

    switch (opts_.command) {
        case Command::Build:   return cmd_build();
        case Command::Clean:   return cmd_clean();
        case Command::Rebuild: return cmd_rebuild();
        case Command::Install: return cmd_install();
        case Command::Run:     return cmd_run();
        case Command::Check:   return cmd_check();
        case Command::Env:     return cmd_env();
        case Command::Version: return cmd_version();
        case Command::Help:    print_help(); return 0;
    }
    return 1;
}

// ── separator ─────────────────────────────────────────────────────────────────
void BuildEngine::separator() const {
    std::cout << c(col::DIM,
        "  ─────────────────────────────────────────────────────────────") << "\n";
}

// ── cmd_version ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_version() {
    print_version();
    return 0;
}

// ── cmd_env ───────────────────────────────────────────────────────────────────
int BuildEngine::cmd_env() {
    try {
        auto env = discover_env(opts_.sdk_override, opts_.ndk_override,
                                opts_.aapt2_override);
        print_env(env);
    } catch (const std::exception& e) {
        std::cerr << c(col::BRED, "Error: ") << e.what() << "\n";
        return 1;
    }
    return 0;
}

// ── cmd_check ─────────────────────────────────────────────────────────────────
int BuildEngine::cmd_check() {
    try {
        CMakeParser parser;
        fs::path cmake_path = project_root_ / opts_.cmake_file;
        auto cfg = parser.parse(cmake_path, project_root_);
        cfg.validate();
        std::cout << c(col::BGREEN, "✓ ") << "kinetic.cmake is valid.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << c(col::BRED, "✗ ") << e.what() << "\n";
        return 1;
    }
}

// ── cmd_clean ─────────────────────────────────────────────────────────────────
int BuildEngine::cmd_clean() {
    fs::path bd = project_root_ / "build";
    if (fs::exists(bd)) {
        std::error_code ec;
        fs::remove_all(bd, ec);
        if (ec) {
            std::cerr << c(col::BRED, "Clean failed: ") << ec.message() << "\n";
            return 1;
        }
        std::cout << c(col::BGREEN, "✓ ") << "build/ removed.\n";
    } else {
        std::cout << "build/ does not exist — nothing to clean.\n";
    }
    return 0;
}

// ── cmd_rebuild ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_rebuild() {
    int r = cmd_clean();
    if (r != 0) return r;
    opts_.command = Command::Build;
    return cmd_build();
}

// ── cmd_build  (DAG-driven) ─────────────────────────────────────────────────
int BuildEngine::cmd_build() {
    // Banner
    std::cout << "\n"
              << c(col::BCYAN, "  ⚡ KINETIC  v1.0  —  Android Build System") << "\n";
    separator();

    try {
        // 1. Register all phases into the DAG.
        Dag dag;
        register_all_phases(dag);

        // 2. Validate edges (cycle detection, topo sort).
        std::string dag_err;
        if (!dag.validate(dag_err)) {
            std::cerr << c(col::BRED, "DAG error: ") << dag_err << "\n";
            return 1;
        }

        const auto order = dag.topo_order();
        std::string names;
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (i) names += " → ";
            names += order[i];
        }
        phase_log("DAG", "Validated " + std::to_string(order.size())
                  + " phases: " + names);

        // 3. Build the PhaseContext.
        PhaseContext ctx;
        ctx.project_root  = project_root_;
        ctx.verbose       = opts_.verbose;
        ctx.quiet         = opts_.quiet;
        ctx.no_color      = opts_.no_color;
        ctx.no_dex        = opts_.no_dex;
        ctx.no_sign       = opts_.no_sign;
        ctx.release_mode  = opts_.release_mode;
        ctx.jobs          = opts_.jobs;
        ctx.abi_filter    = opts_.abi_filter;
        ctx.single_phase  = opts_.single_phase;
        ctx.sdk_override  = opts_.sdk_override;
        ctx.ndk_override  = opts_.ndk_override;
        ctx.aapt2_override = opts_.aapt2_override;
        ctx.cmake_file    = opts_.cmake_file;

        // 4. Execute via the thread-pool executor.
        int jobs = ctx.jobs > 0 ? ctx.jobs
                 : static_cast<int>(std::thread::hardware_concurrency());
        if (jobs < 1) jobs = 1;

        Executor executor(dag, jobs);
        int rc = executor.run(ctx);

        if (rc != 0) return 1;

        separator();

        // 5. Telemetry summary (if enabled).
        if (!ctx.single_phase.empty()) return 0;

        fs::path final_apk = ctx.dirs.output_dir
                           / (ctx.cfg.app_name + "-signed.apk");

        TelemetrySummary summary;
        summary.all_passed = true;
        summary.apk_path  = final_apk;
        if (!final_apk.empty() && fs::exists(final_apk))
            summary.apk_size = file_size(final_apk);

        long long total_ns = 0;
        for (const auto& r : executor.results()) {
            total_ns += r.duration_ns;
            PhaseRecord rec;
            rec.name        = r.phase_name;
            rec.duration_ns = r.duration_ns;
            rec.success     = r.success;
            rec.output_desc = r.output_desc;
            summary.phases.push_back(rec);
        }
        summary.total_ns = total_ns;

        if (ctx.cfg.telemetry && !ctx.quiet) {
            print_telemetry(summary);
        }

        return 0;

    } catch (const std::exception& e) {
        // Error was already printed by fatal(); just return failure.
        return 1;
    }
}

// ── cmd_install ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_install() {
    int r = cmd_build();
    if (r != 0) return r;

    CMakeParser parser;
    fs::path cmake_path = project_root_ / opts_.cmake_file;
    auto cfg = parser.parse(cmake_path, project_root_);

    fs::path out_dir = project_root_ / cfg.output_dir;
    fs::path apk_path;
    if (fs::exists(out_dir)) {
        for (const auto& e : fs::directory_iterator(out_dir))
            if (e.path().extension() == ".apk") { apk_path = e.path(); break; }
    }
    if (apk_path.empty()) {
        std::cerr << c(col::BRED, "Error: ") << "No APK found in " << out_dir << "\n";
        return 1;
    }

    phase_log("INSTALL", "adb install " + apk_path.filename().string());
    return adb({"install", "-r", apk_path.string()});
}

// ── cmd_run ───────────────────────────────────────────────────────────────────
int BuildEngine::cmd_run() {
    int r = cmd_install();
    if (r != 0) return r;

    CMakeParser parser;
    fs::path cmake_path = project_root_ / opts_.cmake_file;
    auto cfg = parser.parse(cmake_path, project_root_);

    std::string activity = cfg.package_name + "/.MainActivity";
    phase_log("RUN", "adb shell am start -n " + activity);
    return adb({"shell", "am", "start", "-n", activity});
}

// ── adb helper ────────────────────────────────────────────────────────────────
int BuildEngine::adb(const std::vector<std::string>& args) {
    std::string adb_path = which("adb");
    if (adb_path.empty()) {
        std::cerr << c(col::BRED, "Error: ") << "adb not found on PATH\n";
        return 1;
    }
    std::vector<std::string> cmd = { adb_path };
    cmd.insert(cmd.end(), args.begin(), args.end());
    auto r = exec_proc(cmd, project_root_);
    if (!r.out.empty()) std::cout << r.out;
    if (!r.err.empty()) std::cerr << r.err;
    return r.exit_code;
}

} // namespace kinetic
