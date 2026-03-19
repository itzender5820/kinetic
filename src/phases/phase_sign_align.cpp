// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_sign_align.cpp  —  Phase 13: SIGN_ALIGN
//
//  Produces exactly ONE output file: <output_name>-signed.apk
//  All intermediate files (unaligned, aligned-but-unsigned) are deleted.
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_sign_align.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

// Delete a file if it exists (silently, used to clean intermediates)
static void remove_if_exists(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p)) fs::remove(p, ec);
}

fs::path phase_sign_align(const KineticConfig& cfg,
                           const KineticEnv& env,
                           const fs::path& project_root,
                           const fs::path& apk_path) {
    if (!fs::exists(apk_path)) {
        fatal("SIGN_ALIGN", err::PKG_004,
              "APK to sign not found", apk_path.string());
    }

    const fs::path output_dir  = apk_path.parent_path();
    const std::string base     = cfg.output_name;

    // All paths derived from the base name — makes it easy to track and clean
    const fs::path aligned_apk = output_dir / (base + "-aligned.apk");
    const fs::path signed_apk  = output_dir / (base + "-signed.apk");

    // ── Clean any leftover files from previous builds ─────────────────────────
    remove_if_exists(aligned_apk);
    remove_if_exists(signed_apk);
    // Also remove any stale .idsig from a previous apksigner run
    remove_if_exists(fs::path(signed_apk.string() + ".idsig"));

    // ── Step 1: zipalign ─────────────────────────────────────────────────────
    fs::path to_sign = apk_path;  // default: sign the raw APK if zipalign absent

    if (!env.zipalign.empty() && fs::exists(env.zipalign)) {
        phase_log("SIGN_ALIGN", "zipalign  →  " + aligned_apk.filename().string());

        auto r = exec_proc({
            env.zipalign.string(),
            "-f", "-v", "4",
            apk_path.string(),
            aligned_apk.string()
        }, project_root);

        if (r.exit_code != 0) {
            std::string detail = r.err.empty() ? r.out : r.err;
            fatal("SIGN_ALIGN", err::SGN_003, "zipalign failed",
                  aligned_apk.string(), detail);
        }
        to_sign = aligned_apk;
    } else {
        phase_warn("SIGN_ALIGN", "zipalign not found — skipping alignment step");
    }

    // ── Step 2: apksigner ────────────────────────────────────────────────────
    if (env.apksigner.empty() || !fs::exists(env.apksigner)) {
        phase_warn("SIGN_ALIGN", "apksigner not found — APK will be unsigned");
        // Rename/move to the canonical signed name so downstream always has one file
        std::error_code ec;
        fs::rename(to_sign, signed_apk, ec);
        if (ec) {
            // rename across filesystems can fail — fall back to copy+delete
            fs::copy_file(to_sign, signed_apk, fs::copy_options::overwrite_existing, ec);
            if (!ec) fs::remove(to_sign, ec);
        }
        return signed_apk;
    }

    // Expand ~ in keystore path
    std::string keystore_path = cfg.keystore;
    if (!keystore_path.empty() && keystore_path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) keystore_path = std::string(home) + keystore_path.substr(1);
    }

    if (!fs::exists(keystore_path)) {
        fatal("SIGN_ALIGN", err::SGN_001,
              "Keystore not found: " + keystore_path, keystore_path);
    }

    phase_log("SIGN_ALIGN", "Signing with " + fs::path(keystore_path).filename().string() + " ...");

    auto r = exec_proc({
        env.apksigner.string(),
        "sign",
        "--ks",            keystore_path,
        "--ks-key-alias",  cfg.key_alias,
        "--ks-pass",       "pass:" + cfg.store_password,
        "--key-pass",      "pass:" + cfg.key_password,
        "--out",           signed_apk.string(),
        to_sign.string()
    }, project_root);

    if (r.exit_code != 0) {
        std::string detail = r.err.empty() ? r.out : r.err;
        fatal("SIGN_ALIGN", err::SGN_002, "apksigner failed",
              keystore_path, detail);
    }

    if (!fs::exists(signed_apk)) {
        fatal("SIGN_ALIGN", err::SGN_002,
              "Signed APK not produced", signed_apk.string());
    }

    // ── Clean up intermediates ────────────────────────────────────────────────
    // Remove the raw unsigned APK (testapp-debug.apk) — only keep the signed one
    if (apk_path != signed_apk) remove_if_exists(apk_path);
    // Remove aligned-but-unsigned intermediate
    if (to_sign != signed_apk && to_sign != apk_path) remove_if_exists(to_sign);

    phase_log("SIGN_ALIGN", "Done  → " + signed_apk.filename().string()
              + "  (signed + aligned)");
    return signed_apk;
}

} // namespace kinetic
