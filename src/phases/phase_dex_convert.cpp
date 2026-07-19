// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_dex_convert.cpp  —  Phase 08: DEX_CONVERT
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_dex_convert.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

static std::string file_size_str(const fs::path& p) {
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    if (ec) return "? bytes";
    if (sz >= 1024 * 1024) return std::to_string(sz / (1024 * 1024)) + " MB";
    if (sz >= 1024) return std::to_string(sz / 1024) + " KB";
    return std::to_string(sz) + " bytes";
}

static fs::path find_d8(const KineticEnv& env) {
    for (auto&& p : {env.d8, env.build_tools_path / "d8"}) {
        if (fs::exists(p)) return p;
    }
    fs::path from_path = fs::path(which("d8"));
    if (!from_path.empty()) return from_path;
    return {};
}

void PhaseDexConvert::execute(PhaseContext& ctx) {
    if (ctx.no_dex) { phase_log("DEX_CONVERT", "--no-dex: skipping dex"); return; }

    fs::path d8 = find_d8(ctx.env);
    if (d8.empty()) fatal("DEX_CONVERT", err::DEX_001,
        "d8 not found", "(in PATH)",
        "Install Android build-tools: sdkmanager build-tools;"
        + std::to_string(ctx.cfg.target_sdk));

    ctx.timer.start("DEX_CONVERT");
    phase_log("DEX_CONVERT", "Converting classes → classes.dex ...");

    std::vector<fs::path> class_files;
    for (const auto& e : fs::recursive_directory_iterator(ctx.dirs.classes_dir))
        if (e.is_regular_file() && e.path().extension() == ".class")
            class_files.push_back(e.path());

    if (class_files.empty()) fatal("DEX_CONVERT", err::DEX_002,
        "No .class files found in " + ctx.dirs.classes_dir.string(),
        ctx.dirs.classes_dir.string(),
        "Ensure java_sources / kotlin_sources are configured correctly");

    std::vector<std::string> cmd = {
        d8.string(),
        "--min-api", std::to_string(ctx.cfg.min_sdk),
        "--output", ctx.dirs.dex_dir.string()
    };
    for (const auto& cf : class_files) cmd.push_back(cf.string());

    auto r = exec_proc(cmd, ctx.project_root);
    if (r.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("DEX_CONVERT", err::DEX_003,
              "d8 failed to produce classes.dex",
              ctx.dirs.dex_dir.string(),
              r.err.empty() ? r.out : r.err);
    }

    fs::path dex = ctx.dirs.dex_dir / "classes.dex";
    if (!fs::exists(dex)) fatal("DEX_CONVERT", err::DEX_002,
        "classes.dex not found after d8 run",
        ctx.dirs.dex_dir.string(),
        "d8 exited with code 0 but did not create classes.dex");

    auto sz = file_size_str(dex);
    phase_log("DEX_CONVERT", "classes.dex → " + dex.string() + " (" + sz + ")");
    ctx.timer.stop(true, "classes.dex (" + sz + ")");
}

} // namespace kinetic
