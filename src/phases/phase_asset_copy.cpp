// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_asset_copy.cpp  —  Phase 10: ASSET_COPY
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_asset_copy.hpp"
#include "../copy/file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

void PhaseAssetCopy::execute(PhaseContext& ctx) {
    const fs::path app_lib = ctx.project_root / "app" / "lib";
    const fs::path assets  = ctx.dirs.staging_dir / "assets" / "kinetic";

    ctx.timer.start("ASSET_COPY");
    phase_log("ASSET_COPY", "Validating app/lib/ → assets/kinetic/ ...");

    if (!fs::exists(app_lib)) {
        phase_warn("ASSET_COPY", "app/lib/ directory not found — skipping");
        ctx.timer.stop(true, "skipped (no app/lib)");
        return;
    }

    int copied = copy_dir_recursive(app_lib, assets, true, "ASSET_COPY");
    phase_log("ASSET_COPY", "All assets OK (" + std::to_string(copied) + " files)");
    ctx.timer.stop(true, std::to_string(copied) + " assets validated");
}

} // namespace kinetic
