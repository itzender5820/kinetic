// ─────────────────────────────────────────────────────────────────────────────
//  core/engine.cpp  —  Pipeline orchestration
// ─────────────────────────────────────────────────────────────────────────────
#include "engine.hpp"
#include "telemetry.hpp"
#include "../config/cmake_parser.hpp"
#include "../env/env_scanner.hpp"
#include "../env/aapt2_resolver.hpp"
#include "../error/error_reporter.hpp"
#include "../utils/process.hpp"
#include "../phases/phase_res_compile.hpp"
#include "../phases/phase_link_aprs.hpp"
#include "../phases/phase_ndk_compile.hpp"
#include "../phases/phase_java_compile.hpp"
#include "../phases/phase_kotlin_compile.hpp"
#include "../phases/phase_dex_convert.hpp"
#include "../phases/phase_so_copy.hpp"
#include "../phases/phase_asset_copy.hpp"
#include "../phases/phase_manifest_merge.hpp"
#include "../phases/phase_apk_pack.hpp"
#include "../phases/phase_sign_align.hpp"
#include "../cli/help_printer.hpp"
#include "../kinetic.hpp"

#include <iostream>
#include <fstream>

namespace kinetic {

// ── Constructor ───────────────────────────────────────────────────────────────
BuildEngine::BuildEngine(const CliOptions& opts, const fs::path& project_root)
    : opts_(opts), project_root_(project_root) {}

// ── Directory setup ───────────────────────────────────────────────────────────
void BuildEngine::setup_dirs() {
    build_dir_   = project_root_ / "build";
    flat_dir_    = build_dir_ / "intermediates" / "flat";
    aprs_dir_    = build_dir_ / "intermediates" / "aprs";
    obj_dir_     = build_dir_ / "intermediates" / "obj";
    lib_dir_     = build_dir_ / "intermediates" / "lib";
    classes_dir_ = build_dir_ / "intermediates" / "classes";
    dex_dir_     = build_dir_ / "intermediates" / "dex";
    staging_dir_ = build_dir_ / "intermediates" / "staging";
    output_dir_  = project_root_ / cfg_.output_dir;
}

// ── Main entry ────────────────────────────────────────────────────────────────
int BuildEngine::run() {
    // Apply --no-color globally
    if (opts_.no_color) g_color_enabled = false;

    switch (opts_.command) {
        case Command::Build:   return cmd_build();
        case Command::Clean:   return cmd_clean();
        case Command::Rebuild: return cmd_rebuild();
        case Command::Install: return cmd_install();
        case Command::Run:     return cmd_run();
        case Command::Check:   return cmd_check();
        case Command::Env:     return cmd_env();
        case Command::Version: return cmd_version();
        case Command::Help:    print_help(); return 0;
    }
    return 1;
}

// ── separator ─────────────────────────────────────────────────────────────────
void BuildEngine::separator() const {
    std::cout << c(col::DIM, "  ─────────────────────────────────────────────────────────────") << "\n";
}

// ── cmd_version ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_version() {
    print_version();
    return 0;
}

// ── cmd_env ───────────────────────────────────────────────────────────────────
int BuildEngine::cmd_env() {
    // For --env we only scan environment, no config needed
    try {
        auto env = discover_env(opts_.sdk_override, opts_.ndk_override,
                                opts_.aapt2_override);
        print_env(env);
    } catch (const std::exception& e) {
        std::cerr << c(col::BRED, "Error: ") << e.what() << "\n";
        return 1;
    }
    return 0;
}

// ── cmd_check ─────────────────────────────────────────────────────────────────
int BuildEngine::cmd_check() {
    try {
        CMakeParser parser;
        fs::path cmake_path = project_root_ / opts_.cmake_file;
        cfg_ = parser.parse(cmake_path, project_root_);
        cfg_.validate();
        std::cout << c(col::BGREEN, "✓ ") << "kinetic.cmake is valid.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << c(col::BRED, "✗ ") << e.what() << "\n";
        return 1;
    }
}

// ── cmd_clean ─────────────────────────────────────────────────────────────────
int BuildEngine::cmd_clean() {
    fs::path bd = project_root_ / "build";
    if (fs::exists(bd)) {
        std::error_code ec;
        fs::remove_all(bd, ec);
        if (ec) {
            std::cerr << c(col::BRED, "Clean failed: ") << ec.message() << "\n";
            return 1;
        }
        std::cout << c(col::BGREEN, "✓ ") << "build/ removed.\n";
    } else {
        std::cout << "build/ does not exist — nothing to clean.\n";
    }
    return 0;
}

// ── cmd_rebuild ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_rebuild() {
    int r = cmd_clean();
    if (r != 0) return r;
    opts_.command = Command::Build;
    return cmd_build();
}

// ── cmd_build ────────────────────────────────────────────────────────────────
int BuildEngine::cmd_build() {
    // Banner
    std::cout << "\n"
              << c(col::BCYAN, "  ⚡ KINETIC  v1.0  —  Android Build System") << "\n";
    separator();

    try {
        // ── Phase 01: ENV_SCAN ────────────────────────────────────────────────
        timer_.start("ENV_SCAN");
        env_ = discover_env(opts_.sdk_override, opts_.ndk_override,
                            opts_.aapt2_override);
        timer_.stop(true, "SDK+NDK resolved");

        // ── Phase 02: CMAKE_PARSE ─────────────────────────────────────────────
        timer_.start("CMAKE_PARSE");
        CMakeParser parser;
        fs::path cmake_path = project_root_ / opts_.cmake_file;
        cfg_ = parser.parse(cmake_path, project_root_);
        cfg_.validate();

        // Apply color from config
        if (!cfg_.color_output) g_color_enabled = false;
        if (opts_.no_color)     g_color_enabled = false;

        // Expand ~ in paths
        const char* home_c = std::getenv("HOME");
        if (home_c) cfg_.resolve_tilde_paths(home_c);

        // Override ABI if specified on CLI
        if (!opts_.abi_filter.empty()) cfg_.abi_filters = { opts_.abi_filter };

        // Re-resolve NDK clang++ for the correct min API
        reresolve_ndk_for_api(env_, cfg_.min_sdk);
        // Update build_tools_ver in env if config specifies one
        if (!cfg_.build_tools_ver.empty() && env_.build_tools_ver != cfg_.build_tools_ver) {
            fs::path bt = env_.sdk_path / "build-tools" / cfg_.build_tools_ver;
            if (fs::exists(bt)) {
                env_.build_tools_path = bt;
                env_.build_tools_ver  = cfg_.build_tools_ver;
                env_.sdk_aapt2        = bt / "aapt2";
                env_.d8               = bt / "d8";
                env_.apksigner        = bt / "apksigner";
                env_.zipalign         = bt / "zipalign";
            }
        }

        setup_dirs();
        timer_.stop(true, std::to_string(24) + " config keys");

        // ── Phase 03: RES_COMPILE ─────────────────────────────────────────────
        timer_.start("RES_COMPILE");
        int res_count = phase_res_compile(cfg_, env_, project_root_, flat_dir_);
        timer_.stop(true, std::to_string(res_count) + " .flat files");

        // ── Phase 04: LINK_APRS ───────────────────────────────────────────────
        timer_.start("LINK_APRS");
        fs::path aprs = phase_link_aprs(cfg_, env_, project_root_, flat_dir_, aprs_dir_);
        timer_.stop(true, "resources.aprs");

        // ── Phase 05: NDK_COMPILE ─────────────────────────────────────────────
        timer_.start("NDK_COMPILE");
        auto so_files = phase_ndk_compile(cfg_, env_, project_root_, obj_dir_, lib_dir_);
        timer_.stop(true, std::to_string(so_files.size()) + " .so files");

        // ── Phase 06: JAVA_COMPILE ────────────────────────────────────────────
        timer_.start("JAVA_COMPILE");
        phase_java_compile(cfg_, env_, project_root_, classes_dir_);
        // Count .class files
        int jclass = 0;
        if (fs::exists(classes_dir_))
            for (const auto& e : fs::recursive_directory_iterator(classes_dir_))
                if (e.path().extension() == ".class") ++jclass;
        timer_.stop(true, std::to_string(jclass) + " .class files");

        // ── Phase 07: KOTLIN_COMPILE ──────────────────────────────────────────
        timer_.start("KOTLIN_COMPILE");
        phase_kotlin_compile(cfg_, env_, project_root_, classes_dir_);
        int kclass = 0;
        if (fs::exists(classes_dir_))
            for (const auto& e : fs::recursive_directory_iterator(classes_dir_))
                if (e.path().extension() == ".class") ++kclass;
        timer_.stop(true, std::to_string(kclass) + " .class files");

        // ── Phase 08: DEX_CONVERT ─────────────────────────────────────────────
        timer_.start("DEX_CONVERT");
        fs::path dex_out;
        if (!opts_.no_dex) {
            dex_out = phase_dex_convert(cfg_, env_, project_root_, classes_dir_, dex_dir_);
        } else {
            phase_log("DEX_CONVERT", "--no-dex flag set — skipping");
        }
        timer_.stop(true, "classes.dex");

        // ── Phase 09: SO_COPY ─────────────────────────────────────────────────
        timer_.start("SO_COPY");
        int so_copied = phase_so_copy(cfg_, env_, project_root_, lib_dir_, staging_dir_);
        timer_.stop(true, std::to_string(so_copied) + " libs staged");

        // ── Phase 10: ASSET_COPY ──────────────────────────────────────────────
        timer_.start("ASSET_COPY");
        int assets_copied = phase_asset_copy(cfg_, env_, project_root_, staging_dir_);
        timer_.stop(true, std::to_string(assets_copied) + " files staged");

        // ── Phase 11: MANIFEST_MERGE ──────────────────────────────────────────
        timer_.start("MANIFEST_MERGE");
        phase_manifest_merge(cfg_, env_, project_root_, staging_dir_);
        timer_.stop(true, "merged manifest");

        // ── Phase 12: APK_PACK ────────────────────────────────────────────────
        timer_.start("APK_PACK");
        fs::path apk = phase_apk_pack(cfg_, env_, project_root_,
                                      staging_dir_, aprs, dex_dir_, output_dir_);
        timer_.stop(true, cfg_.output_name + ".apk");

        // ── Phase 13: SIGN_ALIGN ──────────────────────────────────────────────
        fs::path final_apk = apk;
        timer_.start("SIGN_ALIGN");
        if (!opts_.no_sign) {
            final_apk = phase_sign_align(cfg_, env_, project_root_, apk);
        } else {
            phase_log("SIGN_ALIGN", "--no-sign flag set — skipping");
        }
        timer_.stop(true, "signed + aligned");

        separator();

        // ── Phase 14: TELEMETRY ───────────────────────────────────────────────
        if (cfg_.telemetry && !opts_.quiet) {
            auto summary = make_summary(timer_, final_apk);
            print_telemetry(summary);
        }

        return 0;

    } catch (const std::exception& e) {
        // Error was already printed by fatal(); just return failure.
        return 1;
    }
}

// ── cmd_install ───────────────────────────────────────────────────────────────
int BuildEngine::cmd_install() {
    int r = cmd_build();
    if (r != 0) return r;

    // Find the produced APK
    fs::path out_dir = project_root_ / cfg_.output_dir;
    fs::path apk_path;
    if (fs::exists(out_dir)) {
        for (const auto& e : fs::directory_iterator(out_dir)) {
            if (e.path().extension() == ".apk") { apk_path = e.path(); break; }
        }
    }
    if (apk_path.empty()) {
        std::cerr << c(col::BRED, "Error: ") << "No APK found in " << out_dir << "\n";
        return 1;
    }

    phase_log("INSTALL", "adb install " + apk_path.filename().string());
    return adb({"install", "-r", apk_path.string()});
}

// ── cmd_run ───────────────────────────────────────────────────────────────────
int BuildEngine::cmd_run() {
    int r = cmd_install();
    if (r != 0) return r;

    std::string activity = cfg_.package_name + "/.MainActivity";
    phase_log("RUN", "adb shell am start -n " + activity);
    return adb({"shell", "am", "start", "-n", activity});
}

// ── adb helper ────────────────────────────────────────────────────────────────
int BuildEngine::adb(const std::vector<std::string>& args) {
    std::string adb_path = which("adb");
    if (adb_path.empty()) {
        std::cerr << c(col::BRED, "Error: ") << "adb not found on PATH\n";
        return 1;
    }
    std::vector<std::string> cmd = { adb_path };
    cmd.insert(cmd.end(), args.begin(), args.end());
    auto r = exec_proc(cmd, project_root_);
    if (!r.out.empty()) std::cout << r.out;
    if (!r.err.empty()) std::cerr << r.err;
    return r.exit_code;
}

} // namespace kinetic
