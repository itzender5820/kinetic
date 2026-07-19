// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_apk_pack.cpp  —  Phase 12: APK_PACK
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_apk_pack.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

static fs::path find_aapt2(const KineticEnv& env) {
    for (auto&& p : {env.sdk_aapt2, env.build_tools_path / "aapt2"})
        if (fs::exists(p)) return p;
    fs::path from_path = fs::path(which("aapt2"));
    if (!from_path.empty()) return from_path;
    return {};
}

void PhaseApkPack::execute(PhaseContext& ctx) {
    const fs::path dex       = ctx.dirs.dex_dir / "classes.dex";
    const fs::path stg       = ctx.dirs.staging_dir;
    const fs::path manifest  = stg / "AndroidManifest.xml";
    const fs::path res_dir   = ctx.dirs.aprs_dir;

    ctx.timer.start("APK_PACK");

    // Validate all inputs exist
    if (!fs::exists(manifest)) fatal("APK_PACK", err::PKG_001,
        "staging/AndroidManifest.xml not found",
        manifest.string(),
        "Ensure MANIFEST_MERGE phase has run");
    if (!fs::exists(dex))      fatal("APK_PACK", err::PKG_002,
        "intermediates/dex/classes.dex not found",
        dex.string(),
        "Ensure DEX_CONVERT phase has run");

    fs::path aapt2 = find_aapt2(ctx.env);
    if (aapt2.empty()) fatal("APK_PACK", err::PKG_003,
        "aapt2 not found in $PATH or $ANDROID_HOME/build-tools/",
        "(in PATH)",
        "Ensure ANDROID_HOME is set and build-tools are installed");

    fs::create_directories(ctx.dirs.output_dir);

    std::string unsigned_name = ctx.cfg.app_name + "-unsigned.apk";
    fs::path out = ctx.dirs.output_dir / unsigned_name;

    std::vector<std::string> cmd = {
        aapt2.string(), "link", "--manifest", manifest.string(),
        "-I", (ctx.env.sdk_path / "platforms" / std::to_string(ctx.cfg.target_sdk)
               / "android.jar").string(),
        "-o", out.string()
    };

    if (fs::exists(res_dir) && !fs::is_empty(res_dir)) {
        for (const auto& flat : fs::directory_iterator(res_dir)) {
            if (flat.path().extension() == ".flat") {
                cmd.push_back("-R");
                cmd.push_back(flat.path().string());
            }
        }
    }

    cmd.push_back("--auto-add-overlay");

    auto r = exec_proc(cmd, ctx.project_root);
    if (r.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("APK_PACK", err::PKG_001,
              "aapt2 link failed", out.string(),
              r.err.empty() ? r.out : r.err);
    }

    phase_log("APK_PACK", "Unpacked APK: " + out.string());

    // Use zip to add dex, assets, libs
    auto zip_rm = exec_proc({"/bin/bash", "-c",
        "cd \"" + out.parent_path().string() + "\" && "
        "rm -rf _apk_work && mkdir -p _apk_work"}, ctx.project_root);
    if (zip_rm.exit_code != 0) fatal("APK_PACK", err::PKG_004,
        "Failed to create APK staging dir", out.parent_path().string(),
        zip_rm.err.empty() ? zip_rm.out : zip_rm.err);

    auto zip_add = exec_proc({"/bin/bash", "-c",
        "cd \"" + out.parent_path().string() + "/_apk_work\" && "
        "unzip -qo \"" + out.string() + "\" 2>/dev/null || true && "
        "cp -f \"" + dex.string() + "\" classes.dex && "
        "if [ -d \"" + stg.string() + "/assets\" ]; then "
        "  mkdir -p assets && cp -rf \"" + stg.string() + "/assets/\"* assets/ 2>/dev/null; "
        "fi && "
        "if [ -d \"" + stg.string() + "/lib\" ]; then "
        "  mkdir -p lib && cp -rf \"" + stg.string() + "/lib/\"* lib/ 2>/dev/null; "
        "fi"}, ctx.project_root);
    if (zip_add.exit_code != 0) fatal("APK_PACK", err::PKG_004,
        "Failed to add resources to APK staging",
        out.parent_path().string(),
        zip_add.err.empty() ? zip_add.out : zip_add.err);

    auto zip_repack = exec_proc({"/bin/bash", "-c",
        "cd \"" + out.parent_path().string() + "/_apk_work\" && "
        "rm -f \"" + out.string() + "\" && "
        "zip -r -q \"" + out.string() + "\" ."}, ctx.project_root);
    if (zip_repack.exit_code != 0) fatal("APK_PACK", err::PKG_004,
        "Failed to repackage APK",
        out.string(),
        zip_repack.err.empty() ? zip_repack.out : zip_repack.err);

    auto zip_clean = exec_proc({"/bin/bash", "-c",
        "rm -rf \"" + out.parent_path().string() + "/_apk_work\""},
        ctx.project_root);

    phase_log("APK_PACK", "Raw APK (unsigned, uncompressed): " + out.string());
    ctx.timer.stop(true, unsigned_name);
}

} // namespace kinetic
