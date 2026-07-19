// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_cmake_parse.cpp  —  Phase 02: CMAKE_PARSE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_cmake_parse.hpp"
#include "../config/cmake_parser.hpp"
#include "../env/aapt2_resolver.hpp"
#include "../kinetic.hpp"

namespace kinetic {

void PhaseCmakeParse::execute(PhaseContext& ctx) {
    CMakeParser parser;
    fs::path cmake_path = ctx.project_root / ctx.cmake_file;
    ctx.cfg = parser.parse(cmake_path, ctx.project_root);
    ctx.cfg.validate();

    // Apply color from config
    if (!ctx.cfg.color_output) {
        g_color_enabled = false;
    }
    if (ctx.no_color) {
        g_color_enabled = false;
    }

    // Expand ~ in paths
    const char* home_c = std::getenv("HOME");
    if (home_c) ctx.cfg.resolve_tilde_paths(home_c);

    // Override ABI if specified on CLI
    if (!ctx.abi_filter.empty()) ctx.cfg.abi_filters = { ctx.abi_filter };

    // Re-resolve NDK clang++ for the correct min API
    reresolve_ndk_for_api(ctx.env, ctx.cfg.min_sdk);

    // Update build_tools_ver in env if config specifies one
    if (!ctx.cfg.build_tools_ver.empty()
            && ctx.env.build_tools_ver != ctx.cfg.build_tools_ver) {
        fs::path bt = ctx.env.sdk_path / "build-tools" / ctx.cfg.build_tools_ver;
        if (fs::exists(bt)) {
            ctx.env.build_tools_path = bt;
            ctx.env.build_tools_ver  = ctx.cfg.build_tools_ver;
            ctx.env.sdk_aapt2        = bt / "aapt2";
            ctx.env.d8               = bt / "d8";
            ctx.env.apksigner        = bt / "apksigner";
            ctx.env.zipalign         = bt / "zipalign";
        }
    }

    // Build directory layout — all phases reference ctx.dirs.*
    ctx.dirs.build_dir   = ctx.project_root / "build";
    ctx.dirs.flat_dir    = ctx.dirs.build_dir / "intermediates" / "flat";
    ctx.dirs.aprs_dir    = ctx.dirs.build_dir / "intermediates" / "aprs";
    ctx.dirs.obj_dir     = ctx.dirs.build_dir / "intermediates" / "obj";
    ctx.dirs.lib_dir     = ctx.dirs.build_dir / "intermediates" / "lib";
    ctx.dirs.classes_dir = ctx.dirs.build_dir / "intermediates" / "classes";
    ctx.dirs.dex_dir     = ctx.dirs.build_dir / "intermediates" / "dex";
    ctx.dirs.staging_dir = ctx.dirs.build_dir / "intermediates" / "staging";
    ctx.dirs.output_dir  = ctx.project_root / ctx.cfg.output_dir;
    ctx.dirs.cache_dir   = ctx.dirs.build_dir / ".kinetic-cache";
    ctx.dirs.stamps_file = ctx.dirs.cache_dir / "stamps.tsv";

    // Create all required build directories up front.
    std::error_code ec;
    fs::create_directories(ctx.dirs.flat_dir,    ec);
    fs::create_directories(ctx.dirs.aprs_dir,    ec);
    fs::create_directories(ctx.dirs.obj_dir,     ec);
    fs::create_directories(ctx.dirs.lib_dir,     ec);
    fs::create_directories(ctx.dirs.classes_dir, ec);
    fs::create_directories(ctx.dirs.dex_dir,     ec);
    fs::create_directories(ctx.dirs.staging_dir, ec);
    fs::create_directories(ctx.dirs.output_dir,  ec);
    fs::create_directories(ctx.dirs.cache_dir,   ec);
}

} // namespace kinetic
