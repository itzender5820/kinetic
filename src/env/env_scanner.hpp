#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  env/env_scanner.hpp  —  Android SDK/NDK auto-discovery
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <filesystem>

namespace kinetic {

namespace fs = std::filesystem;

struct KineticEnv {
    bool     sdk_found          = false;
    bool     ndk_found          = false;

    fs::path sdk_path;
    fs::path ndk_path;
    fs::path build_tools_path;
    std::string build_tools_ver;

    // Tool binaries
    fs::path sdk_aapt2;         // SDK-bundled AAPT2 (for res compile + link)
    fs::path sys_aapt2;         // System/aarch64 AAPT2 (for APK packaging)
    fs::path d8;                // d8 dex converter
    fs::path apksigner;         // APK signer
    fs::path zipalign;          // zipalign

    // NDK toolchain
    fs::path ndk_clangxx;       // aarch64 clang++
    fs::path ndk_sysroot;       // NDK sysroot
    std::string ndk_triple;     // e.g. "aarch64-linux-android"
    std::string api_level_str;  // e.g. "34"

    // Optionals on PATH
    std::string javac_path;
    std::string kotlinc_path;

    // kotlin-stdlib.jar — found relative to kotlinc binary.
    // Passed to d8 as input so its classes land in classes.dex.
    fs::path kotlin_stdlib;
};

// Scan $HOME for Android SDK and NDK. On failure, calls kinetic::fatal().
// If the user passed overrides, those paths are used directly and verified.
KineticEnv discover_env(const std::string& sdk_override  = "",
                        const std::string& ndk_override  = "",
                        const std::string& aapt2_override = "",
                        const std::string& build_tools_ver_hint = "");

// Print the discovered environment to stdout (for --env command)
void print_env(const KineticEnv& env);

} // namespace kinetic
