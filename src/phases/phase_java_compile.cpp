// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_java_compile.cpp  —  Phase 06: JAVA_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_java_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

namespace kinetic {

fs::path phase_java_compile(const KineticConfig& cfg,
                             const KineticEnv& env,
                             const fs::path& project_root,
                             const fs::path& classes_dir) {
    fs::path java_src = project_root / cfg.java_sources;

    if (!fs::exists(java_src)) {
        phase_log("JAVA_COMPILE", "No Java sources found — skipping");
        return classes_dir;
    }

    if (env.javac_path.empty()) {
        fatal("JAVA_COMPILE", err::JVM_001,
              "javac not found on PATH",
              "",
              "Install Java: sudo apt install openjdk-17-jdk");
    }

    // Collect .java files
    std::vector<std::string> java_files;
    for (const auto& entry : fs::recursive_directory_iterator(java_src)) {
        if (entry.is_regular_file() && entry.path().extension() == ".java") {
            java_files.push_back(entry.path().string());
        }
    }

    if (java_files.empty()) {
        phase_log("JAVA_COMPILE", "Java source dir exists but no .java files — skipping");
        return classes_dir;
    }

    phase_log("JAVA_COMPILE", "Compiling " + std::to_string(java_files.size()) + " .java files ...");

    std::error_code ec;
    fs::create_directories(classes_dir, ec);

    // android.jar classpath
    std::string api_str = std::to_string(cfg.compile_sdk);
    fs::path android_jar = env.sdk_path / "platforms"
                         / ("android-" + api_str) / "android.jar";

    std::vector<std::string> cmd;
    cmd.push_back(env.javac_path);
    cmd.push_back("-source"); cmd.push_back(cfg.java_version);
    cmd.push_back("-target"); cmd.push_back(cfg.java_version);
    cmd.push_back("-classpath"); cmd.push_back(android_jar.string());
    cmd.push_back("-d"); cmd.push_back(classes_dir.string());
    for (const auto& f : java_files) cmd.push_back(f);

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("JAVA_COMPILE", err::JVM_002,
              "javac compilation failed",
              java_src.string(),
              detail);
    }

    // Count produced .class files
    int class_count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(classes_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".class") ++class_count;
    }

    phase_log("JAVA_COMPILE", "Done  (" + std::to_string(class_count) + " .class files)");
    return classes_dir;
}

} // namespace kinetic
