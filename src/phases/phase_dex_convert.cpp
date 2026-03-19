// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_dex_convert.cpp  —  Phase 08: DEX_CONVERT
//  Convert .class files → classes.dex using d8
//
//  CRITICAL: kotlin-stdlib.jar must be passed as a d8 INPUT (not --lib).
//  When kotlinc compiles .kt files the output .class files contain bytecode
//  that calls kotlin.jvm.internal.Intrinsics and other stdlib classes.
//  If those classes are not in classes.dex the app crashes at launch with:
//    NoClassDefFoundError: Failed resolution of: Lkotlin/jvm/internal/Intrinsics
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_dex_convert.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

fs::path phase_dex_convert(const KineticConfig& cfg,
                            const KineticEnv& env,
                            const fs::path& project_root,
                            const fs::path& classes_dir,
                            const fs::path& dex_dir) {
    // Collect app .class files
    std::vector<std::string> class_files;
    if (fs::exists(classes_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(classes_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".class")
                class_files.push_back(entry.path().string());
        }
    }

    if (class_files.empty()) {
        phase_log("DEX_CONVERT", "No .class files — skipping DEX conversion");
        return dex_dir;
    }

    if (env.d8.empty() || !fs::exists(env.d8)) {
        fatal("DEX_CONVERT", err::DEX_001,
              "d8 not found in build-tools",
              env.build_tools_path.string());
    }

    std::error_code ec;
    fs::create_directories(dex_dir, ec);

    const std::string api_str   = std::to_string(cfg.min_sdk);
    const fs::path    android_jar = env.sdk_path / "platforms"
                                  / ("android-" + std::to_string(cfg.compile_sdk))
                                  / "android.jar";

    // ── Check for kotlin-stdlib ───────────────────────────────────────────────
    const bool has_kotlin_stdlib = !env.kotlin_stdlib.empty()
                                && fs::exists(env.kotlin_stdlib);
    if (!has_kotlin_stdlib) {
        // Check if any .kt-derived class is present (META-INF/main.kotlin_module)
        const fs::path kotlin_module = classes_dir / "META-INF" / "main.kotlin_module";
        if (fs::exists(kotlin_module)) {
            phase_warn("DEX_CONVERT",
                "Kotlin classes detected but kotlin-stdlib.jar not found.\n"
                "         The APK will crash at runtime with NoClassDefFoundError.\n"
                "         Expected: " + (env.kotlinc_path.empty() ? "<kotlinc>/lib/" :
                    fs::path(env.kotlinc_path).parent_path().parent_path().string()
                    + "/lib/") + "kotlin-stdlib.jar");
        }
    }

    phase_log("DEX_CONVERT",
              "Converting " + std::to_string(class_files.size()) + " .class files"
              + (has_kotlin_stdlib ? " + kotlin-stdlib" : " (no kotlin-stdlib)")
              + " → .dex ...");

    std::vector<std::string> cmd;
    cmd.push_back(env.d8.string());
    cmd.push_back("--min-api"); cmd.push_back(api_str);
    cmd.push_back("--output");  cmd.push_back(dex_dir.string());

    // android.jar is a --lib (bootclasspath), NOT an input — its classes are
    // already on-device and must NOT be dexed into our APK.
    if (fs::exists(android_jar)) {
        cmd.push_back("--lib"); cmd.push_back(android_jar.string());
    }

    // kotlin-stdlib IS an input — its classes must be dexed into classes.dex
    // so they are available at runtime on the device.
    if (has_kotlin_stdlib) {
        cmd.push_back(env.kotlin_stdlib.string());
        phase_log("DEX_CONVERT", "  + " + env.kotlin_stdlib.filename().string());
    }

    // App .class files
    for (const auto& f : class_files) cmd.push_back(f);

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("DEX_CONVERT", err::DEX_001,
              "d8 dex conversion failed",
              classes_dir.string(),
              detail);
    }

    const fs::path dex_out = dex_dir / "classes.dex";
    if (!fs::exists(dex_out)) {
        fatal("DEX_CONVERT", err::DEX_002,
              "classes.dex not produced by d8",
              dex_dir.string());
    }

    // Report final dex size for awareness (kotlin-stdlib adds ~1.5 MB)
    const auto dex_size = fs::file_size(dex_out);
    phase_log("DEX_CONVERT",
              "Done  → classes.dex  ("
              + std::to_string(dex_size / 1024) + " KB)");
    return dex_out;
}

} // namespace kinetic
