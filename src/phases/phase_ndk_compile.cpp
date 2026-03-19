// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_ndk_compile.cpp  —  Phase 05: NDK_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_ndk_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

#include <sstream>

namespace kinetic {

// ABI → NDK target triple mapping
static std::string abi_to_triple(const std::string& abi) {
    if (abi == "arm64-v8a")   return "aarch64-linux-android";
    if (abi == "armeabi-v7a") return "armv7a-linux-androideabi";
    if (abi == "x86_64")      return "x86_64-linux-android";
    if (abi == "x86")         return "i686-linux-android";
    return "aarch64-linux-android";
}

// Find the correct versioned clang++ for a given ABI and API level
static fs::path find_clangxx(const KineticEnv& env,
                              const std::string& abi,
                              int api) {
    fs::path llvm = env.ndk_path / "toolchains" / "llvm" / "prebuilt";
    fs::path prebuilt_host;
    for (const auto& e : fs::directory_iterator(llvm)) {
        if (e.is_directory()) { prebuilt_host = e.path(); break; }
    }
    if (prebuilt_host.empty()) return env.ndk_clangxx;

    fs::path bin = prebuilt_host / "bin";
    std::string triple = abi_to_triple(abi);
    std::string api_str = std::to_string(api);

    // armeabi-v7a uses armv7a triple but android prefix
    if (abi == "armeabi-v7a") triple = "armv7a-linux-androideabi";

    fs::path candidate = bin / (triple + api_str + "-clang++");
    if (fs::exists(candidate)) return candidate;

    candidate = bin / (triple + "-clang++");
    if (fs::exists(candidate)) return candidate;

    return env.ndk_clangxx;
}

// Compile a single source file → .o
static fs::path compile_source(const fs::path& src_file,
                                const fs::path& obj_dir,
                                const fs::path& clangxx,
                                const KineticConfig& cfg,
                                const KineticEnv& env,
                                const std::string& abi,
                                const fs::path& project_root) {
    // Build .o path mirroring source layout
    fs::path rel = src_file.is_absolute()
                   ? fs::relative(src_file, project_root)
                   : src_file;
    fs::path obj_path = obj_dir / abi / (rel.string() + ".o");
    fs::create_directories(obj_path.parent_path());

    std::vector<std::string> cmd;
    cmd.push_back(clangxx.string());

    // C or C++?
    auto ext = src_file.extension().string();
    bool is_c = (ext == ".c");

    if (!is_c) {
        cmd.push_back("-std=c++" + std::to_string(cfg.cpp_standard));
    }

    // Parse and append cpp_flags
    {
        std::istringstream ss(cfg.cpp_flags);
        std::string tok;
        while (ss >> tok) cmd.push_back(tok);
    }

    // Every object going into a .so must be position-independent.
    // -fPIC on the compile command is mandatory; -fPIC on the link step alone
    // is insufficient and produces R_AARCH64_ADR_PREL_PG_HI21 relocation errors.
    cmd.push_back("-fPIC");

    // Target triple encodes the API level (e.g. aarch64-linux-android24).
    // Clang derives __ANDROID_API__ from --target automatically.
    cmd.push_back("--target=" + abi_to_triple(abi) + std::to_string(cfg.min_sdk));

    // Sysroot
    cmd.push_back("--sysroot=" + env.ndk_sysroot.string());

    // Include directories
    for (const auto& inc : cfg.include_dirs) {
        fs::path inc_path = project_root / inc;
        cmd.push_back("-I" + inc_path.string());
    }
    // NDK unified headers
    cmd.push_back("-I" + (env.ndk_sysroot / "usr" / "include").string());

    cmd.push_back("-c");
    cmd.push_back(src_file.is_absolute() ? src_file.string()
                                         : (project_root / src_file).string());
    cmd.push_back("-o");
    cmd.push_back(obj_path.string());

    phase_log("NDK_COMPILE", (is_c ? "clang " : "clang++ ")
              + src_file.filename().string() + "  [" + abi + "]");

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("NDK_COMPILE", err::NDK_003,
              "Compilation failed: " + src_file.filename().string(),
              src_file.string(),
              detail);
    }
    return obj_path;
}

// Link .o files → shared library
static fs::path link_shared(const std::vector<fs::path>& obj_files,
                              const fs::path& lib_dir,
                              const fs::path& clangxx,
                              const KineticConfig& cfg,
                              const KineticEnv& env,
                              const std::string& abi,
                              const fs::path& project_root) {
    // Derive library name from app_name
    std::string lib_name = "lib" + cfg.app_name + ".so";
    fs::path out_so = lib_dir / abi / lib_name;
    fs::create_directories(out_so.parent_path());

    std::vector<std::string> cmd;
    cmd.push_back(clangxx.string());
    cmd.push_back("-shared");
    cmd.push_back("-fPIC");

    // ── 16KB page size support (required for Android 15+ devices) ──────────
    // -z max-page-size=16384 aligns ELF LOAD segments to 16KB boundaries.
    // Without this flag, apps crash on devices with 16KB page granules
    // (Pixel 9 and later with CONFIG_ARM64_4K_PAGES=n kernels).
    // The flag is safe on 4KB-page devices — it just wastes a few KB of padding.
    cmd.push_back("-Wl,-z,max-page-size=16384");
    cmd.push_back("--target=" + abi_to_triple(abi) + std::to_string(cfg.min_sdk));
    cmd.push_back("--sysroot=" + env.ndk_sysroot.string());

    for (const auto& obj : obj_files) cmd.push_back(obj.string());

    // NDK system lib dirs
    cmd.push_back("-L" + (env.ndk_sysroot / "usr" / "lib"
                           / (abi_to_triple(abi) + std::to_string(cfg.min_sdk))).string());

    for (const auto& lib : cfg.link_libs) {
        cmd.push_back("-l" + lib);
    }

    cmd.push_back("-o"); cmd.push_back(out_so.string());

    phase_log("NDK_COMPILE", "Linking → " + lib_name + "  [" + abi + "]");

    auto result = exec_proc(cmd, project_root);
    if (result.exit_code != 0) {
        std::string detail = result.err.empty() ? result.out : result.err;
        fatal("NDK_COMPILE", err::NDK_004,
              "Linker error: " + lib_name,
              out_so.string(),
              detail);
    }
    return out_so;
}

// ── Public entry point ────────────────────────────────────────────────────────
std::vector<fs::path> phase_ndk_compile(const KineticConfig& cfg,
                                         const KineticEnv& env,
                                         const fs::path& project_root,
                                         const fs::path& obj_dir,
                                         const fs::path& lib_dir) {
    if (cfg.sources.empty()) {
        phase_log("NDK_COMPILE", "No C++ sources — skipping NDK compile");
        return {};
    }

    std::vector<fs::path> all_sos;

    for (const auto& abi : cfg.abi_filters) {
        phase_log("NDK_COMPILE", "ABI: " + abi);

        fs::path clangxx = find_clangxx(env, abi, cfg.min_sdk);
        phase_log("NDK_COMPILE", "Compiler → " + clangxx.filename().string());

        // Compile each source
        std::vector<fs::path> obj_files;
        for (const auto& src : cfg.sources) {
            fs::path src_path = project_root / src;
            if (!fs::exists(src_path)) {
                fatal("NDK_COMPILE", err::NDK_002,
                      "Source file not found: " + src,
                      src_path.string(),
                      "Verify path in KINETIC_SOURCES in kinetic.cmake");
            }
            obj_files.push_back(
                compile_source(src_path, obj_dir, clangxx, cfg, env, abi, project_root)
            );
        }

        // Link into shared library
        fs::path so = link_shared(obj_files, lib_dir, clangxx, cfg, env, abi, project_root);
        all_sos.push_back(so);
        phase_log("NDK_COMPILE", "Done  → " + so.filename().string());
    }

    return all_sos;
}

} // namespace kinetic
