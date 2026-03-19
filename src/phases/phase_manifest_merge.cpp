// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_manifest_merge.cpp  —  Phase 11: MANIFEST_MERGE
//  Validate AndroidManifest.xml and copy into staging directory
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_manifest_merge.hpp"
#include "../copy/file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

#include <fstream>
#include <sstream>

namespace kinetic {

// Minimal validation: check that the file contains <manifest and package=
static void validate_manifest(const fs::path& manifest_path,
                               const std::string& expected_package) {
    std::ifstream f(manifest_path);
    if (!f.is_open()) {
        fatal("MANIFEST_MERGE", err::PKG_002,
              "Cannot open AndroidManifest.xml",
              manifest_path.string());
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    if (content.find("<manifest") == std::string::npos) {
        fatal("MANIFEST_MERGE", err::PKG_003,
              "AndroidManifest.xml missing <manifest> root element",
              manifest_path.string(),
              "Ensure the file starts with: <manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">");
    }

    if (!expected_package.empty() && content.find(expected_package) == std::string::npos) {
        phase_warn("MANIFEST_MERGE",
                   "package=\"" + expected_package + "\" not found in AndroidManifest.xml");
    }
}

fs::path phase_manifest_merge(const KineticConfig& cfg,
                               const KineticEnv& env,
                               const fs::path& project_root,
                               const fs::path& staging_dir) {
    (void)env;

    fs::path manifest_src = project_root / "src" / "main" / "AndroidManifest.xml";

    if (!fs::exists(manifest_src)) {
        fatal("MANIFEST_MERGE", err::PKG_002,
              "AndroidManifest.xml not found",
              manifest_src.string());
    }

    phase_log("MANIFEST_MERGE", "Validating AndroidManifest.xml ...");
    validate_manifest(manifest_src, cfg.package_name);

    // Copy into staging root
    std::error_code ec;
    fs::create_directories(staging_dir, ec);
    fs::path manifest_dst = staging_dir / "AndroidManifest.xml";

    copy_file_validated(manifest_src, manifest_dst, cfg.verbose_copy, true, "MANIFEST_MERGE");
    phase_log("MANIFEST_MERGE", "Done  → merged manifest");

    return manifest_dst;
}

} // namespace kinetic
