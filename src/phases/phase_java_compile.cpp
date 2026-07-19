// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_java_compile.cpp  —  Phase 06: JAVA_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_java_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <thread>
#include <mutex>

namespace kinetic {

static std::mutex compile_mtx;

static fs::path find_javac(const KineticEnv& env) {
    if (!env.javac_path.empty()) {
        fs::path h = fs::path(env.javac_path) / "bin" / "javac";
        if (fs::exists(h)) return h;
    }
    fs::path from_path = fs::path(which("javac"));
    if (!from_path.empty()) return from_path;
    for (auto&& p : {"/usr/bin/javac", "/usr/local/bin/javac",
                     "/Library/Java/JavaVirtualMachines/jdk-21.jdk/Contents/Home/bin/javac",
                     "/opt/homebrew/opt/openjdk@21/bin/javac"}) {
        if (fs::exists(p)) return p;
    }
    return {};
}

static std::string source_version_flag(int v) {
    switch (v) { case 17: return "--release"; case 16: return "--release";
        case 15: case 14: case 13: case 12: case 11: return "--release";
        case 8: return "-source 8 -target 8"; default: return "--release";
    }
}

static std::string target_version_flag(int v) {
    return std::to_string(v);
}

void PhaseJavaCompile::execute(PhaseContext& ctx) {
    const fs::path src_dir = ctx.project_root / "src" / "main" / "java";
    if (!fs::exists(src_dir)) {
        phase_warn("JAVA_COMPILE", "No java sources in src/main/java/ — skipping");
        return;
    }

    fs::path javac = find_javac(ctx.env);
    if (javac.empty()) fatal("JAVA_COMPILE", err::JVM_001,
        "javac not found", "(in PATH)",
        "Install a JDK: brew install openjdk@21");

    std::vector<fs::path> java_files;
    for (const auto& e : fs::recursive_directory_iterator(src_dir))
        if (e.is_regular_file() && e.path().extension() == ".java")
            java_files.push_back(e.path());

    if (java_files.empty()) {
        phase_warn("JAVA_COMPILE", "No .java files found under src/main/java/");
        return;
    }

    ctx.timer.start("JAVA_COMPILE");
    phase_log("JAVA_COMPILE", "Found " + std::to_string(java_files.size())
              + " java sources — compiling ...");

    fs::create_directories(ctx.dirs.classes_dir);

    std::error_code ec;
    std::vector<fs::path> compiled_files;
    ctx.compile_classes_.clear();

    for (const auto& jf : java_files) {
        std::vector<std::string> cmd = {
            javac.string(),
            "-classpath", (ctx.env.sdk_path / "platforms" / std::to_string(ctx.cfg.target_sdk)
                / "android.jar").string(),
            "-d", ctx.dirs.classes_dir.string(),
            "-proc:none",
            "-Xlint:none"
        };

        int ver = 11;
        try { ver = std::stoi(ctx.cfg.java_version); } catch (...) {}
        if (ver == 8) { cmd.push_back("-source"); cmd.push_back("8");
                        cmd.push_back("-target"); cmd.push_back("8");
        } else {
            cmd.push_back(source_version_flag(ver));
            cmd.push_back(target_version_flag(ver));
        }
        cmd.push_back(jf.string());

        auto r = exec_proc(cmd, ctx.project_root);
        if (r.exit_code != 0) {
            std::lock_guard lk(compile_mtx);
            ctx.timer.stop(false, "failed");
            fatal("JAVA_COMPILE", err::JVM_002,
                  "javac failed: " + jf.filename().string(),
                  jf.string(), r.err.empty() ? r.out : r.err);
        }

        std::lock_guard lk(compile_mtx);
        compiled_files.push_back(jf);
        phase_log("JAVA_COMPILE",
                  fs::relative(jf, src_dir).string() + " → .class");
    }

    ctx.compile_classes_ = compiled_files;
    phase_log("JAVA_COMPILE", "Compiled " + std::to_string(compiled_files.size()) + " sources");
    ctx.timer.stop(true, std::to_string(compiled_files.size()) + " sources compiled");
}

} // namespace kinetic
