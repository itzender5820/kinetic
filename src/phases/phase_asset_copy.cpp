// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_asset_copy.cpp  —  Phase 10: ASSET_COPY
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_asset_copy.hpp"
#include "../copy/file_copier.hpp"
#include "../copy/essential_manifest.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

int phase_asset_copy(const KineticConfig& cfg,
                     const KineticEnv& env,
                     const fs::path& project_root,
                     const fs::path& staging_dir) {
    int total = 0;

    // ── assets/ ──────────────────────────────────────────────────────────────
    fs::path assets_src = project_root / "src" / "main" / "assets";
    if (fs::exists(assets_src)) {
        fs::path assets_dst = staging_dir / "assets";
        int n = copy_dir_recursive(assets_src, assets_dst, cfg.verbose_copy, "ASSET_COPY");
        phase_log("ASSET_COPY", "assets/  →  " + std::to_string(n) + " files");
        total += n;
    }

    // ── res/raw/ ─────────────────────────────────────────────────────────────
    fs::path raw_src = project_root / "src" / "main" / "res" / "raw";
    if (fs::exists(raw_src)) {
        fs::path raw_dst = staging_dir / "res" / "raw";
        int n = copy_dir_recursive(raw_src, raw_dst, cfg.verbose_copy, "ASSET_COPY");
        phase_log("ASSET_COPY", "res/raw/  →  " + std::to_string(n) + " files");
        total += n;
    }

    // ── Extra assets from kinetic.cmake ───────────────────────────────────────
    for (const auto& extra : cfg.extra_assets) {
        fs::path src = project_root / extra;
        fs::path dst = staging_dir / "assets" / fs::path(extra).filename();
        copy_file_validated(src, dst, cfg.verbose_copy, true, "ASSET_COPY");
        ++total;
    }

    // ── Essential silent files (libc++_shared.so, fonts, prebuilts) ──────────
    auto manifest = build_essential_manifest(cfg, env, staging_dir, project_root);
    int ess = copy_essential_files(manifest, cfg.verbose_copy);
    total += ess;

    phase_log("ASSET_COPY", "Done  (" + std::to_string(total) + " files staged)");
    return total;
}

} // namespace kinetic
