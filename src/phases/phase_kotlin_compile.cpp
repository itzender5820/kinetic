// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_kotlin_compile.cpp  —  Phase 07: KOTLIN_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_kotlin_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

fs::path phase_kotlin_compile(const KineticConfig& cfg,
                               const KineticEnv& env,
                               const fs::path& project_root,
                               const fs::path& classes_dir) {
    fs::path kotlin_src = project_root / cfg.kotlin_sources;

    if (!fs::exists(kotlin_src)) {
        phase_log("KOTLIN_COMPILE", "No Kotlin sources found — skipping");
        return classes_dir;
    }

    if (env.kotlinc_path.empty()) {
        fatal("KOTLIN_COMPILE", err::JVM_003,
              "kotlinc not found on PATH",
              "",
              "Install Kotlin: snap install kotlin  or download from kotlinlang.org");
    }

    std::vector<std::string> kt_files;
    for (const auto& entry : fs::recursive_directory_iterator(kotlin_src)) {
        if (entry.is_regular_file() && entry.path().extension() == ".kt")
            kt_files.push_back(entry.path().string());
    }

    if (kt_files.empty()) {
        phase_log("KOTLIN_COMPILE", "Kotlin source dir exists but no .kt files — skipping");
        return classes_dir;
    }

    phase_log("KOTLIN_COMPILE",
              "Compiling " + std::to_string(kt_files.size()) + " .kt files ...");

    std::error_code ec;
    fs::create_directories(classes_dir, ec);

    const std::string api_str   = std::to_string(cfg.compile_sdk);
    const fs::path android_jar  = env.sdk_path / "platforms"
                                / ("android-" + api_str) / "android.jar";

    // Classpath = android.jar + already-compiled Java classes (for cross-language refs)
    const std::string classpath = android_jar.string() + ":" + classes_dir.string();

    // ── Fix: suppress jansi native-library loading ──────────────────────────
    // kotlinc (all versions including 2.x) ships jansi for terminal colours.
    // On Termux it tries to dlopen a glibc .so ("libc.so.6") which doesn't
    // exist on Android/Bionic. This makes the JVM log an error and in some
    // versions exit non-zero before compiling anything.
    //
    // JAVA_TOOL_OPTIONS is read by the JVM itself before any Java code runs —
    // it's more reliable than -J flags which depend on the kotlinc shell script.
    // Setting it in the child process environment avoids polluting the parent.
    //
    // We pass it via the environment of exec_proc by prepending the assignment
    // to the shell command and using env(1).
    const std::string java_opts =
        "JAVA_TOOL_OPTIONS=-Djansi.passthrough=true -Dkotlinx.coroutines.debug=off";

    std::vector<std::string> cmd;
    // Use env(1) to set JAVA_TOOL_OPTIONS only for this child process
    cmd.push_back("env");
    cmd.push_back(java_opts);
    cmd.push_back(env.kotlinc_path);
    cmd.push_back("-classpath"); cmd.push_back(classpath);
    cmd.push_back("-d");         cmd.push_back(classes_dir.string());
    cmd.push_back("-jvm-target"); cmd.push_back(cfg.java_version);

    for (const auto& f : kt_files) cmd.push_back(f);

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;

        // Strip the jansi warning from the detail so it doesn't obscure real errors
        auto strip = [](std::string& s, const std::string& prefix) {
            auto pos = s.find(prefix);
            if (pos != std::string::npos) {
                auto end = s.find('\n', pos);
                s.erase(pos, (end == std::string::npos ? s.size() : end + 1) - pos);
            }
        };
        strip(detail, "Failed to load native library:jansi");
        strip(detail, "java.lang.UnsatisfiedLinkError");
        strip(detail, "osinfo: Linux/arm64");

        fatal("KOTLIN_COMPILE", err::JVM_004,
              "kotlinc compilation failed",
              kotlin_src.string(),
              detail);
    }

    int class_count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(classes_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".class")
            ++class_count;
    }

    phase_log("KOTLIN_COMPILE", "Done  (" + std::to_string(class_count) + " .class files)");
    return classes_dir;
}

} // namespace kinetic
