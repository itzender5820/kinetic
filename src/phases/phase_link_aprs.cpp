// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_link_aprs.cpp  —  Phase 04: LINK_APRS
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_link_aprs.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

static fs::path find_aprs_resource_sh() {
    const char* exe_env = std::getenv("KINETIC_BIN");
    fs::path exe_dir    = fs::path(exe_env ? std::string(exe_env)
                                            : std::string("/usr/local/bin")).parent_path();
    for (auto&& rel : {"resources.sh", "../share/kinetic/resources.sh",
                       "share/kinetic/resources.sh"}) {
        fs::path p = exe_dir / rel;
        if (fs::exists(p)) return p;
    }
    const char* prefix = std::getenv("PREFIX");
    if (prefix) {
        fs::path p = fs::path(prefix) / "share" / "kinetic" / "resources.sh";
        if (fs::exists(p)) return p;
    }
    return {};
}

void PhaseLinkAprs::execute(PhaseContext& ctx) {
    const fs::path out_aprs = ctx.dirs.aprs_dir / "resources.aprs";

    ctx.timer.start("LINK_APRS");
    phase_log("LINK_APRS", "Linking resources.aprs ...");

    fs::path resources_sh = find_aprs_resource_sh();
    if (resources_sh.empty()) {
        phase_warn("LINK_APRS",
            "resources.sh not found — falling back to simple symlink");
        if (fs::exists(out_aprs)) fs::remove(out_aprs);
        fs::create_directories(out_aprs.parent_path());
        fs::path src = ctx.dirs.flat_dir / "resources.flat";
        if (fs::exists(src)) {
            fs::copy_file(src, out_aprs);
        } else {
            phase_warn("LINK_APRS",
                "No resources.flat found — skipping link");
        }
        ctx.timer.stop(true, "skipped");
        return;
    }

    fs::path res_dir   = ctx.project_root / "src" / "main" / "res";
    fs::path android_jar = ctx.env.sdk_path / "platforms" / std::to_string(ctx.cfg.target_sdk)
                         / "android.jar";
    if (!fs::exists(android_jar)) {
        fatal("LINK_APRS", err::RES_004,
              "android.jar not found for API " + std::to_string(ctx.cfg.target_sdk),
              android_jar.string(),
              "Run: $KINETIC_HOME/bin/sdkmanager \"platforms;"
              + std::to_string(ctx.cfg.target_sdk) + "\"");
    }

    std::vector<std::string> cmd = {
        "/bin/bash", resources_sh.string(),
        ctx.dirs.flat_dir.string(),
        out_aprs.string(),
        res_dir.string(),
        android_jar.string()
    };

    auto r = exec_proc(cmd, ctx.project_root);
    if (r.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("LINK_APRS", err::RES_010,
              "resources.sh failed to link resources.aprs",
              out_aprs.string(), r.err.empty() ? r.out : r.err);
    }

    if (fs::exists(out_aprs))
        phase_log("LINK_APRS", "resources.aprs → " + out_aprs.string());
    else
        phase_warn("LINK_APRS", "resources.aprs not produced — res may be empty");

    ctx.timer.stop(true, "linked resources.aprs");
}

} // namespace kinetic
