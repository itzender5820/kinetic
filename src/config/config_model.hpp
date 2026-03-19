#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  config_model.hpp  —  KineticConfig struct (parsed from kinetic.cmake)
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>

namespace kinetic {

struct KineticConfig {
    // ── Project identity ───────────────────────────────────────────────────
    std::string app_name         = "MyApplication";
    std::string package_name;           // REQUIRED
    int         version_code     = 1;
    std::string version_name     = "1.0.0";

    // ── SDK / NDK ──────────────────────────────────────────────────────────
    int         min_sdk          = 21;
    int         target_sdk       = 34;
    int         compile_sdk      = 34;
    std::string build_tools_ver  = "";  // Empty = auto-detect latest

    // ── ABI targets ────────────────────────────────────────────────────────
    std::vector<std::string> abi_filters = { "arm64-v8a" };

    // ── Language config ────────────────────────────────────────────────────
    int         cpp_standard     = 17;
    std::string cpp_flags        = "-O2 -Wall -Wextra";
    std::string java_version     = "11";
    std::string kotlin_version   = "1.9";

    // ── C++ sources ────────────────────────────────────────────────────────
    std::vector<std::string> sources;       // KINETIC_SOURCES
    std::vector<std::string> include_dirs;  // KINETIC_INCLUDE_DIRS
    std::vector<std::string> link_libs;     // KINETIC_LINK_LIBS
    std::vector<std::string> prebuilt_libs; // KINETIC_PREBUILT_LIBS (.so)

    // ── Java / Kotlin sources ──────────────────────────────────────────────
    std::string java_sources    = "src/main/java";
    std::string kotlin_sources  = "src/main/kotlin";

    // ── Signing ────────────────────────────────────────────────────────────
    std::string keystore        = "~/.android/debug.keystore";
    std::string key_alias       = "androiddebugkey";
    std::string key_password    = "android";
    std::string store_password  = "android";

    // ── Output ─────────────────────────────────────────────────────────────
    std::string output_dir      = "build/outputs";
    std::string output_name     = "app-debug";

    // ── Telemetry options ──────────────────────────────────────────────────
    bool        telemetry       = true;
    bool        verbose_copy    = true;
    bool        color_output    = true;

    // ── Extra assets ───────────────────────────────────────────────────────
    std::vector<std::string> extra_assets;

    // ── Derived helpers (filled in after parsing) ──────────────────────────
    std::string project_root;   // Absolute path to kinetic.cmake directory

    // Expand ~ in paths
    void resolve_tilde_paths(const std::string& home);

    // Validate required fields; throws on error
    void validate() const;
};

} // namespace kinetic
