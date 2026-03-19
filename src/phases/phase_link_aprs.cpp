// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_link_aprs.cpp  —  Phase 04: LINK_APRS
//  Link compiled .flat resource files into resources.aprs using SDK AAPT2
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_link_aprs.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <unistd.h>
#include <sys/utsname.h>

namespace kinetic {

// Same arch-detection helpers as phase_res_compile (duplicated to keep phases
// self-contained and avoid a shared utility header for these two statics).
static bool link_is_x86_elf_error(const std::string& s) {
    return s.find("Syntax error: word unexpected") != std::string::npos
        || s.find("Exec format error")             != std::string::npos
        || s.find("cannot execute binary file")    != std::string::npos;
}
static bool link_host_is_aarch64() {
    struct utsname u{}; uname(&u);
    std::string m(u.machine);
    return m == "aarch64" || m == "arm64";
}
static std::string link_find_native_aapt2() {
    char buf[4096]{};
    if (readlink("/proc/self/exe", buf, sizeof(buf)-1) > 0) {
        fs::path sd = fs::path(std::string(buf)).parent_path();
        for (auto&& rel : { "assets/aapt2-aarch64", "../assets/aapt2-aarch64" }) {
            fs::path p = sd / rel;
            if (fs::exists(p)) return p.string();
        }
    }
    // $PREFIX/bin/aapt2 (Termux / any prefix install)
    const char* prefix_e = std::getenv("PREFIX");
    std::string prefix_aapt2 = prefix_e ? std::string(prefix_e) + "/bin/aapt2" : "";
    if (!prefix_aapt2.empty() && access(prefix_aapt2.c_str(), X_OK) == 0) return prefix_aapt2;
    return which("aapt2");
}

fs::path phase_link_aprs(const KineticConfig& cfg,
                          const KineticEnv& env,
                          const fs::path& project_root,
                          const fs::path& flat_dir,
                          const fs::path& aprs_dir) {
    std::error_code ec;
    fs::create_directories(aprs_dir, ec);

    fs::path manifest_xml = project_root / "src" / "main" / "AndroidManifest.xml";
    if (!fs::exists(manifest_xml)) {
        fatal("LINK_APRS", err::PKG_002, "AndroidManifest.xml not found",
              manifest_xml.string());
    }

    std::string api_str = std::to_string(cfg.compile_sdk);
    fs::path android_jar = env.sdk_path / "platforms"
                         / ("android-" + api_str) / "android.jar";
    fs::path output_aprs = aprs_dir / "resources.aprs";

    std::vector<std::string> flat_files;
    if (fs::exists(flat_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(flat_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".flat")
                flat_files.push_back(entry.path().string());
        }
    }

    if (flat_files.empty()) {
        phase_warn("LINK_APRS", "No .flat files to link — skipping AAPT2 link");
        return output_aprs;
    }

    phase_log("LINK_APRS", "Linking " + std::to_string(flat_files.size())
              + " .flat files → resources.aprs ...");

    // Start with the configured aapt2; auto-correct if it's a wrong-arch binary
    std::string aapt2 = env.sdk_aapt2.string();

    auto build_cmd = [&]() -> std::vector<std::string> {
        std::vector<std::string> cmd;
        cmd.push_back(aapt2);
        cmd.insert(cmd.end(), {
            "link",
            "-o",                  output_aprs.string(),
            "-I",                  android_jar.string(),
            "--manifest",          manifest_xml.string(),
            "--min-sdk-version",   std::to_string(cfg.min_sdk),
            "--target-sdk-version",std::to_string(cfg.target_sdk),
            "--version-code",      std::to_string(cfg.version_code),
            "--version-name",      cfg.version_name,
            "--auto-add-overlay"
        });
        for (const auto& f : flat_files) cmd.push_back(f);
        return cmd;
    };

    auto result = exec_proc(build_cmd(), project_root);

    // Auto-correct for wrong-arch binary on aarch64
    if (result.exit_code != 0 && link_host_is_aarch64()) {
        std::string combined = result.err + result.out;
        if (link_is_x86_elf_error(combined)) {
            std::string native = link_find_native_aapt2();
            if (!native.empty() && native != aapt2) {
                phase_warn("LINK_APRS",
                    "sdk_aapt2 is x86_64 — switching to native: " + native);
                aapt2 = native;
                result = exec_proc(build_cmd(), project_root);
            }
        }
    }

    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("LINK_APRS", err::RES_010, "aapt2 link failed",
              output_aprs.string(), detail);
    }

    if (!fs::exists(output_aprs)) {
        fatal("LINK_APRS", err::RES_011, "resources.aprs not produced by aapt2",
              output_aprs.string());
    }

    phase_log("LINK_APRS", "Done  → resources.aprs");
    return output_aprs;
}

} // namespace kinetic
