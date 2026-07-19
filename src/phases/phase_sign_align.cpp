// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_sign_align.cpp  —  Phase 13: SIGN_ALIGN
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_sign_align.hpp"
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

static fs::path find_apksigner(const KineticEnv& env) {
    for (auto&& p : {env.apksigner, env.build_tools_path / "apksigner"})
        if (fs::exists(p)) return p;
    fs::path from_path = fs::path(which("apksigner"));
    if (!from_path.empty()) return from_path;
    return {};
}

static fs::path find_zipalign(const KineticEnv& env) {
    for (auto&& p : {env.zipalign, env.build_tools_path / "zipalign"})
        if (fs::exists(p)) return p;
    fs::path from_path = fs::path(which("zipalign"));
    if (!from_path.empty()) return from_path;
    return {};
}

static fs::path get_debug_keystore(const KineticConfig& cfg) {
    if (!cfg.keystore.empty())
        return cfg.keystore;
    const char* home_c = std::getenv("HOME");
    if (!home_c) return {};
    fs::path home(home_c);
    fs::path debug_ks = home / ".android" / "debug.keystore";
    if (fs::exists(debug_ks)) return debug_ks;
    fs::path kinetic_ks = home / ".kinetic" / "debug.keystore";
    if (fs::exists(kinetic_ks)) return kinetic_ks;
    fs::create_directories(kinetic_ks.parent_path());
    std::string keytool_cmd =
        "keytool -genkeypair -v -keystore \"" + kinetic_ks.string()
        + "\" -alias androiddebugkey -storepass android -keypass android "
        + "-keyalg RSA -keysize 2048 -validity 10000 "
        + "-dname \"CN=Android Debug,O=Android,C=US\" 2>/dev/null";
    int rc = std::system(keytool_cmd.c_str());
    if (rc == 0 && fs::exists(kinetic_ks)) return kinetic_ks;
    return {};
}

void PhaseSignAlign::execute(PhaseContext& ctx) {
    ctx.timer.start("SIGN_ALIGN");

    fs::path unsigned_apk = ctx.dirs.output_dir / (ctx.cfg.app_name + "-unsigned.apk");
    if (!fs::exists(unsigned_apk)) fatal("SIGN_ALIGN", err::PKG_004,
        "Unsigned APK not found", unsigned_apk.string(),
        "Ensure APK_PACK phase completed");

    fs::path zipalign = find_zipalign(ctx.env);
    if (zipalign.empty()) fatal("SIGN_ALIGN", err::SGN_001,
        "zipalign not found", "(in PATH)",
        "Ensure ANDROID_HOME/build-tools/ is installed");

    fs::path apksigner = find_apksigner(ctx.env);
    if (apksigner.empty()) fatal("SIGN_ALIGN", err::SGN_002,
        "apksigner not found", "(in PATH)",
        "Ensure ANDROID_HOME/build-tools/ is installed");

    fs::path aligned   = ctx.dirs.output_dir / (ctx.cfg.app_name + "-aligned.apk");
    fs::path signed_apk = ctx.dirs.output_dir / (ctx.cfg.app_name + "-signed.apk");

    fs::path keystore = get_debug_keystore(ctx.cfg);
    if (keystore.empty()) fatal("SIGN_ALIGN", err::SGN_003,
        "Keystore not found", "~/.android/debug.keystore",
        "Either configure signing_keystore or install Android Studio");

    auto align = exec_proc({zipalign.string(), "-f", "4",
        unsigned_apk.string(), aligned.string()}, ctx.project_root);
    if (align.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("SIGN_ALIGN", err::SGN_004,
              "zipalign failed", aligned.string(),
              align.err.empty() ? align.out : align.err);
    }
    phase_log("SIGN_ALIGN", "zipalign → " + aligned.string());

    std::string ks_pass = ctx.cfg.store_password.empty() ? "android"
                        : ctx.cfg.store_password;
    std::string key_pass = ctx.cfg.key_password.empty() ? "android"
                         : ctx.cfg.key_password;
    std::string key_alias = ctx.cfg.key_alias.empty() ? "androiddebugkey"
                          : ctx.cfg.key_alias;

    auto sign = exec_proc({apksigner.string(), "sign",
        "--ks", keystore.string(),
        "--ks-pass", "pass:" + ks_pass,
        "--key-pass", "pass:" + key_pass,
        "--ks-key-alias", key_alias,
        "--out", signed_apk.string(),
        aligned.string()}, ctx.project_root);
    if (sign.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("SIGN_ALIGN", err::SGN_002,
              "apksigner sign failed", signed_apk.string(),
              sign.err.empty() ? sign.out : sign.err);
    }
    phase_log("SIGN_ALIGN", "apksigner → " + signed_apk.string());

    auto verify = exec_proc({apksigner.string(), "verify",
        "--verbose", signed_apk.string()}, ctx.project_root);
    if (verify.exit_code != 0) {
        phase_warn("SIGN_ALIGN", "apksigner verify failed: "
            + (verify.err.empty() ? verify.out : verify.err));
    }

    std::error_code ec;
    fs::remove(aligned, ec);
    fs::remove(unsigned_apk, ec);

    auto sz = file_size_str(signed_apk);
    phase_log("SIGN_ALIGN", "Final APK → " + signed_apk.string() + " (" + sz + ")");
    ctx.timer.stop(true, ctx.cfg.app_name + "-signed.apk (" + sz + ")");
}

} // namespace kinetic
