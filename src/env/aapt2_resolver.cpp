// ─────────────────────────────────────────────────────────────────────────────
//  env/aapt2_resolver.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "aapt2_resolver.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

void reresolve_ndk_for_api(KineticEnv& env, int min_sdk) {
    fs::path llvm = env.ndk_path / "toolchains" / "llvm" / "prebuilt";
    if (!fs::exists(llvm)) return;

    fs::path prebuilt_host;
    for (const auto& e : fs::directory_iterator(llvm)) {
        if (e.is_directory()) { prebuilt_host = e.path(); break; }
    }
    if (prebuilt_host.empty()) return;

    fs::path bin = prebuilt_host / "bin";
    std::string api_str = std::to_string(min_sdk);

    // Try versioned clang++ first
    fs::path clangxx = bin / ("aarch64-linux-android" + api_str + "-clang++");
    if (!fs::exists(clangxx)) clangxx = bin / "aarch64-linux-android-clang++";
    if (!fs::exists(clangxx)) clangxx = bin / "clang++";

    if (fs::exists(clangxx)) {
        env.ndk_clangxx   = clangxx;
        env.api_level_str = api_str;
        phase_log("ENV_SCAN", "NDK clang++ resolved for API " + api_str
                  + " → " + clangxx.filename().string());
    }
}

} // namespace kinetic
