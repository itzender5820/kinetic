// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_kotlin_compile.cpp  —  Phase 07: KOTLIN_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_kotlin_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <thread>
#include <mutex>

namespace kinetic {

static std::mutex kmtx;
static int kcompiled = 0;

static fs::path find_kotlinc() {
    std::string found = which("kotlinc");
    if (!found.empty() && fs::exists(found)) return found;
    for (auto&& s : {"/usr/local/bin/kotlinc",
                     "/opt/homebrew/bin/kotlinc",
                     "/usr/bin/kotlinc"}) {
        if (fs::exists(s)) return s;
    }
    return {};
}

void PhaseKotlinCompile::execute(PhaseContext& ctx) {
    if (ctx.cfg.kotlin_sources.empty()) {
        phase_log("KOTLIN_COMPILE", "No kotlin_sources configured — skipping");
        return;
    }

    fs::path kotlinc = find_kotlinc();
    if (kotlinc.empty()) fatal("KOTLIN_COMPILE", err::JVM_004,
        "kotlinc not found", "(in PATH)",
        "Install Kotlin: brew install kotlin");

    fs::path android_jar = ctx.env.sdk_path / "platforms"
                         / std::to_string(ctx.cfg.target_sdk) / "android.jar";
    if (!fs::exists(android_jar)) fatal("KOTLIN_COMPILE", err::JVM_002,
        "android.jar not found", android_jar.string(),
        "Run: $KINETIC_HOME/bin/sdkmanager \"platforms;"
        + std::to_string(ctx.cfg.target_sdk) + "\"");

    std::vector<fs::path> all_kotlin_files;
    {
        fs::path p = ctx.project_root / ctx.cfg.kotlin_sources;
        if (!fs::exists(p)) { phase_warn("KOTLIN_COMPILE", "Missing kotlin_sources path: " + ctx.cfg.kotlin_sources); }
        else if (!fs::is_directory(p)) { all_kotlin_files.push_back(p); }
        else {
            for (const auto& e : fs::recursive_directory_iterator(p))
                if (e.is_regular_file() && e.path().extension() == ".kt")
                    all_kotlin_files.push_back(e.path());
        }
    }

    if (all_kotlin_files.empty()) { phase_log("KOTLIN_COMPILE", "No .kt sources found — skipping"); return; }

    ctx.timer.start("KOTLIN_COMPILE");
    phase_log("KOTLIN_COMPILE", "Compiling " + std::to_string(all_kotlin_files.size()) + " Kotlin sources ...");

    fs::path kotlin_out = ctx.dirs.build_dir / "kotlin_out";
    fs::create_directories(kotlin_out);

    std::string cp = android_jar.string();
    for (const auto& c : ctx.compile_classes_) {
        fs::path parent = c.parent_path();
        bool found = false;
        for (auto& s : {cp, parent.string()}) {
            if (s == parent.string()) { found = true; break; }
        }
        if (!found) cp += ":" + parent.string();
    }

    kcompiled = 0;
    auto compile_batch = [&](const std::vector<fs::path>& files, bool is_multi_abi) {
        std::vector<std::string> cmd = {
            kotlinc.string(),
            "-classpath", cp,
            "-d", kotlin_out.string(),
            "-jvm-target", "17",
            "-nowarn", "-Xskip-prerelease-check",
            "-Xskip-metadata-version-check"
        };
        if (is_multi_abi) cmd.push_back("-Xuse-k2=false");
        for (const auto& f : files) cmd.push_back(f.string());

        auto r = exec_proc(cmd, ctx.project_root);
        std::lock_guard lk(kmtx);
        if (r.exit_code != 0) {
            ctx.timer.stop(false, "failed");
            fatal("KOTLIN_COMPILE", err::JVM_002,
                  "kotlinc failed for batch of "
                  + std::to_string(files.size()) + " files",
                  kotlinc.string(), r.err.empty() ? r.out : r.err);
        }
        kcompiled += static_cast<int>(files.size());
    };

    bool is_multi_abi = ctx.cfg.abi_filters.size() > 1;
    constexpr std::size_t BATCH = 128;
    std::vector<std::thread> workers;
    for (std::size_t i = 0; i < all_kotlin_files.size(); i += BATCH) {
        std::size_t end = std::min(i + BATCH, all_kotlin_files.size());
        std::vector<fs::path> batch(all_kotlin_files.begin() + static_cast<std::ptrdiff_t>(i),
                                    all_kotlin_files.begin() + static_cast<std::ptrdiff_t>(end));
        workers.emplace_back(compile_batch, std::move(batch), is_multi_abi);
        if (workers.size() >= 2) {
            for (auto& w : workers) w.join();
            workers.clear();
        }
    }
    for (auto& w : workers) w.join();
    workers.clear();

    if (kcompiled == 0) { phase_log("KOTLIN_COMPILE", "No Kotlin sources compiled — skipping merge"); return; }

    phase_log("KOTLIN_COMPILE", "Merging into class files ...");
    auto merge = exec_proc({"/bin/bash", "-c",
        "find " + kotlin_out.string() + " -name '*.class' | while read f; do "
        "  dest=\"" + ctx.dirs.classes_dir.string() + "/$(dirname \"${f#" + kotlin_out.string() + "/}\")\"; "
        "  mkdir -p \"$dest\"; cp -f \"$f\" \"$dest/\"; "
        "done"}, ctx.project_root);
    if (merge.exit_code != 0) {
        ctx.timer.stop(false, "failed");
        fatal("KOTLIN_COMPILE", err::JVM_002,
              "Kotlin class merge failed", kotlin_out.string(),
              merge.err.empty() ? merge.out : merge.err);
    }

    phase_log("KOTLIN_COMPILE", "Done  (" + std::to_string(kcompiled) + " files compiled)");
    ctx.timer.stop(true, std::to_string(kcompiled) + " .kt files compiled");
}

} // namespace kinetic
