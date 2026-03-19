// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_apk_pack.cpp  —  Phase 12: APK_PACK
//  Package the distributable APK using the aarch64 system AAPT2
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_apk_pack.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

fs::path phase_apk_pack(const KineticConfig& cfg,
                         const KineticEnv& env,
                         const fs::path& project_root,
                         const fs::path& staging_dir,
                         const fs::path& aprs_path,
                         const fs::path& dex_dir,
                         const fs::path& output_dir) {
    std::error_code ec;
    fs::create_directories(output_dir, ec);

    fs::path apk_out = output_dir / (cfg.output_name + ".apk");

    // Remove any APK from a previous build so we start fresh
    { std::error_code _ec; if (fs::exists(apk_out)) fs::remove(apk_out, _ec); }
    phase_log("APK_PACK", "Packaging " + cfg.output_name + ".apk ...");

    // aapt2 package: use the aarch64 system aapt2 for the final APK
    fs::path manifest = staging_dir / "AndroidManifest.xml";
    if (!fs::exists(manifest)) {
        fatal("APK_PACK", err::PKG_002,
              "AndroidManifest.xml missing from staging",
              manifest.string());
    }

    std::string api_str = std::to_string(cfg.compile_sdk);
    fs::path android_jar = env.sdk_path / "platforms"
                         / ("android-" + api_str) / "android.jar";

    // Collect .flat files again (for package phase using sys aapt2)
    std::vector<std::string> extra_args;

    // Add dex file if present
    fs::path dex_file = dex_dir / "classes.dex";

    // Build aapt2 link command for final APK packaging
    std::vector<std::string> cmd;
    cmd.push_back(env.sys_aapt2.string());
    cmd.push_back("link");
    cmd.push_back("-o"); cmd.push_back(apk_out.string());
    cmd.push_back("-I"); cmd.push_back(android_jar.string());
    cmd.push_back("--manifest"); cmd.push_back(manifest.string());
    cmd.push_back("--min-sdk-version"); cmd.push_back(std::to_string(cfg.min_sdk));
    cmd.push_back("--target-sdk-version"); cmd.push_back(std::to_string(cfg.target_sdk));
    cmd.push_back("--version-code"); cmd.push_back(std::to_string(cfg.version_code));
    cmd.push_back("--version-name"); cmd.push_back(cfg.version_name);
    cmd.push_back("--auto-add-overlay");

    // Always gather and use all .flat files directly.
    // The pre-linked .aprs produced by SDK aapt2 (Phase 04) is a linked APK (ZIP),
    // which aapt2 link -R does NOT accept (it expects magic 'AAPT').
    fs::path flat_dir = project_root / "build" / "intermediates" / "flat";
    if (fs::exists(flat_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(flat_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".flat") {
                cmd.push_back(entry.path().string());
            }
        }
    }

    // Include staging directory as an additional overlay directory
    cmd.push_back("--java"); cmd.push_back((output_dir / "gen").string());

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("APK_PACK", err::PKG_001,
              "aapt2 package failed",
              apk_out.string(),
              detail);
    }

    // If APK was produced, manually add native libs, dex, and assets using zip
    // since aapt2 link handles resources but we need to add the compiled artifacts.
    // We use the 'zip' command-line tool available on all POSIX systems.
    if (!fs::exists(apk_out)) {
        fatal("APK_PACK", err::PKG_004,
              "APK not produced by aapt2",
              apk_out.string());
    }

    // Add classes.dex
    if (fs::exists(dex_file)) {
        auto r = exec_proc({"zip", "-j", apk_out.string(), dex_file.string()}, output_dir);
        if (r.exit_code != 0) {
            phase_warn("APK_PACK", "Failed to add classes.dex to APK");
        }
    }

    // Add lib/ tree (native .so files)
    fs::path lib_staging = staging_dir / "lib";
    if (fs::exists(lib_staging)) {
        // zip -r apk.apk lib/
        auto r = exec_proc({"zip", "-r", apk_out.string(), "lib"}, staging_dir);
        if (r.exit_code != 0) {
            phase_warn("APK_PACK", "Failed to add native libs to APK");
        }
    }

    // Add assets/ tree
    fs::path assets_staging = staging_dir / "assets";
    if (fs::exists(assets_staging)) {
        auto r = exec_proc({"zip", "-r", apk_out.string(), "assets"}, staging_dir);
        if (r.exit_code != 0) {
            phase_warn("APK_PACK", "Failed to add assets to APK");
        }
    }

    phase_log("APK_PACK", "Done  → " + cfg.output_name + ".apk");
    return apk_out;
}

} // namespace kinetic
