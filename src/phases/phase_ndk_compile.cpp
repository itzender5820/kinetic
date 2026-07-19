// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_ndk_compile.cpp  —  Phase 05: NDK_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_ndk_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <algorithm>
#include <thread>

namespace kinetic {

static std::string find_cmake() {
    std::string found = which("cmake");
    if (!found.empty() && fs::exists(found)) return found;
    for (auto&& s : {"/usr/bin/cmake", "/usr/local/bin/cmake"}) {
        if (fs::exists(s)) return s;
    }
    return {};
}

void PhaseNdkCompile::execute(PhaseContext& ctx) {
    const fs::path native_dir   = ctx.project_root / "src" / "main" / "cpp";
    const fs::path build_sub    = ctx.dirs.build_dir / "cmake_build";

    ctx.timer.start("NDK_COMPILE");

    // Validate native source exists
    if (!fs::exists(native_dir)) {
        phase_warn("NDK_COMPILE",
            "src/main/cpp/ not found — skipping NDK compile");
        ctx.timer.stop(true, "skipped (no native source)");
        return;
    }

    fs::path cmake = find_cmake();
    if (cmake.empty()) fatal("NDK_COMPILE", err::NDK_001,
        "cmake not found in $PATH or /usr/bin/",
        "(in PATH)", "Install CMake: brew install cmake");

    fs::path ndk_root = ctx.env.ndk_path;
    if (!fs::exists(ndk_root)) fatal("NDK_COMPILE", err::NDK_002,
        "NDK home not found", ndk_root.string(),
        "Install the NDK via sdkmanager");

    fs::path android_jar = ctx.env.sdk_path / "platforms"
                         / std::to_string(ctx.cfg.target_sdk) / "android.jar";
    fs::path ke = ctx.project_root / "ke";

    phase_log("NDK_COMPILE",
        "Generating CMake project with NDK → " + ndk_root.string());

    fs::create_directories(build_sub);

    // Generate with -DCMAKE_TOOLCHAIN_FILE and all flags
    std::vector<std::string> gen_cmd = {
        cmake.string(),
        "-S", native_dir.string(),
        "-B", build_sub.string(),
        "-DCMAKE_TOOLCHAIN_FILE=" + ndk_root.string() + "/build/cmake/android.toolchain.cmake",
        "-DANDROID_ABI=" + ctx.cfg.abi_filters[0],
        "-DANDROID_PLATFORM=android-" + std::to_string(ctx.cfg.min_sdk),
        "-DANDROID_STL=c++_shared",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_C_FLAGS=-fPIC",
        "-DCMAKE_CXX_FLAGS=-fPIC",
        "-DKINETIC_PROJECT_DIR=" + ctx.project_root.string(),
        "-DKINETIC_JAR=" + android_jar.string(),
        "-DKETC=" + ke.string(),
        "-DKETC_ANDROID_API=" + std::to_string(ctx.cfg.min_sdk),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    };

    auto gen = exec_proc(gen_cmd, ctx.project_root);
    if (gen.exit_code != 0) {
        ctx.timer.stop(false, "failed (generate)");
        fatal("NDK_COMPILE", err::NDK_003,
              "CMake generate failed",
              build_sub.string(), gen.err.empty() ? gen.out : gen.err);
    }

    phase_log("NDK_COMPILE", "Building native .so files ...");

    // Build each ABI
    for (const auto& abi : ctx.cfg.abi_filters) {
        phase_log("NDK_COMPILE", "ABI " + abi + " ...");
        std::vector<std::string> build_cmd = {
            cmake.string(),
            "--build", build_sub.string(),
            "--target", "kinetic",
            "-j", std::to_string(std::max(1u, std::thread::hardware_concurrency() / 2))
        };
        auto build = exec_proc(build_cmd, ctx.project_root);
        if (build.exit_code != 0) {
            ctx.timer.stop(false, "failed (build)");
            fatal("NDK_COMPILE", err::NDK_004,
                  "NDK build failed for ABI " + abi,
                  build_sub.string(),
                  build.err.empty() ? build.out : build.err);
        }

        // Copy output .so → lib/<abi>/
        fs::path abi_lib_dir = ctx.dirs.lib_dir / abi;
        fs::create_directories(abi_lib_dir);

        fs::path so_file = build_sub / "libkinetic.so";
        if (fs::exists(so_file)) {
            fs::copy_file(so_file, abi_lib_dir / "libkinetic.so",
                         fs::copy_options::overwrite_existing);
        }
    }

    phase_log("NDK_COMPILE", "Native build complete.");
    ctx.timer.stop(true, "native .so files built");
}

} // namespace kinetic
