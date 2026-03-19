// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_so_copy.cpp  —  Phase 09: SO_COPY
//  Copy compiled .so files into staging area with ABI mapping validation
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_so_copy.hpp"
#include "../copy/file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

int phase_so_copy(const KineticConfig& cfg,
                  const KineticEnv& env,
                  const fs::path& project_root,
                  const fs::path& lib_dir,
                  const fs::path& staging_dir) {
    int copied = 0;

    // Copy NDK-compiled .so files
    for (const auto& abi : cfg.abi_filters) {
        fs::path abi_lib_dir = lib_dir / abi;
        if (!fs::exists(abi_lib_dir)) continue;

        fs::path staging_abi = staging_dir / "lib" / abi;
        std::error_code ec;
        fs::create_directories(staging_abi, ec);

        for (const auto& entry : fs::directory_iterator(abi_lib_dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".so") continue;

            fs::path dst = staging_abi / entry.path().filename();
            copy_file_validated(entry.path(), dst,
                                cfg.verbose_copy, true, "SO_COPY");
            phase_log("SO_COPY", entry.path().filename().string()
                      + "  →  lib/" + abi + "/  ✓");
            ++copied;
        }
    }

    // Copy prebuilt .so files
    for (const auto& prebuilt : cfg.prebuilt_libs) {
        fs::path src = project_root / prebuilt;
        if (!fs::exists(src)) {
            fatal("SO_COPY", err::CPY_001,
                  "Prebuilt .so not found: " + prebuilt,
                  src.string());
        }

        // Determine ABI from directory component
        std::string matched_abi;
        for (const auto& abi : cfg.abi_filters) {
            if (prebuilt.find(abi) != std::string::npos) {
                matched_abi = abi;
                break;
            }
        }
        if (matched_abi.empty()) {
            phase_warn("SO_COPY", "Could not determine ABI for prebuilt: " + prebuilt
                       + " — defaulting to arm64-v8a");
            matched_abi = "arm64-v8a";
        }

        fs::path staging_abi = staging_dir / "lib" / matched_abi;
        std::error_code ec;
        fs::create_directories(staging_abi, ec);

        fs::path dst = staging_abi / src.filename();
        copy_file_validated(src, dst, cfg.verbose_copy, true, "SO_COPY");
        phase_log("SO_COPY", src.filename().string()
                  + "  →  lib/" + matched_abi + "/  ✓");
        ++copied;
    }

    phase_log("SO_COPY", "Done  (" + std::to_string(copied) + " libs staged)");
    return copied;
}

} // namespace kinetic
