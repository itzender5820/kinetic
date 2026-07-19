// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_so_copy.cpp  —  Phase 09: SO_COPY
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_so_copy.hpp"
#include "../copy/file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

void PhaseSoCopy::execute(PhaseContext& ctx) {
    ctx.timer.start("SO_COPY");
    phase_log("SO_COPY", "Validating and staging native .so files ...");

    for (const auto& abi : ctx.cfg.abi_filters) {
        fs::path src_so = ctx.dirs.lib_dir / abi / "libkinetic.so";
        if (!fs::exists(src_so)) fatal("SO_COPY", err::CPY_004,
            "libkinetic.so not found for ABI " + abi, src_so.string(),
            "Ensure NDK build was successful and lib/<abi>/libkinetic.so exists");

        fs::path dst_so = ctx.dirs.staging_dir / "lib" / abi / "libkinetic.so";
        std::error_code ec;
        fs::create_directories(dst_so.parent_path(), ec);

        copy_file_validated(src_so, dst_so, true, true, "SO_COPY");

        phase_log("SO_COPY", "libkinetic.so → " + abi);
    }

    phase_log("SO_COPY", "Native .so validation complete");
    ctx.timer.stop(true, "libkinetic.so validated");
}

} // namespace kinetic
