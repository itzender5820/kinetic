// ─────────────────────────────────────────────────────────────────────────────
//  copy/essential_manifest.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "essential_manifest.hpp"
#include "file_copier.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

namespace kinetic {

std::vector<EssentialFile> build_essential_manifest(
    const KineticConfig& cfg,
    const KineticEnv& env,
    const fs::path& staging_dir,
    const fs::path& project_root)
{
    std::vector<EssentialFile> manifest;

    // For each ABI, add libc++_shared.so from the NDK sysroot
    for (const auto& abi : cfg.abi_filters) {
        // Map ABI → NDK triple directory name
        std::string ndk_arch_dir;
        if      (abi == "arm64-v8a")   ndk_arch_dir = "aarch64-linux-android";
        else if (abi == "armeabi-v7a") ndk_arch_dir = "arm-linux-androideabi";
        else if (abi == "x86_64")      ndk_arch_dir = "x86_64-linux-android";
        else if (abi == "x86")         ndk_arch_dir = "i686-linux-android";
        else continue;

        // libc++_shared.so location in NDK sysroot
        fs::path libcxx = env.ndk_sysroot / "usr" / "lib" / ndk_arch_dir / "libc++_shared.so";
        if (!fs::exists(libcxx)) {
            // Try alternate layout
            libcxx = env.ndk_path / "sources" / "cxx-stl" / "llvm-libc++" / "libs" / abi / "libc++_shared.so";
        }

        manifest.push_back({
            "libc++_shared.so (" + abi + ")",
            libcxx,
            staging_dir / "lib" / abi / "libc++_shared.so",
            false  // warn but don't fatal — not all apps need shared STL
        });
    }

    // Prebuilt .so libraries from kinetic.cmake
    for (const auto& prebuilt : cfg.prebuilt_libs) {
        fs::path src = project_root / prebuilt;
        // Determine ABI from path component
        std::string abi = "arm64-v8a"; // default
        for (const auto& a : cfg.abi_filters) {
            if (prebuilt.find(a) != std::string::npos) { abi = a; break; }
        }
        manifest.push_back({
            fs::path(prebuilt).filename().string() + " (" + abi + ")",
            src,
            staging_dir / "lib" / abi / fs::path(prebuilt).filename(),
            true
        });
    }

    // Font files: res/font/*.ttf, *.otf
    fs::path font_dir = project_root / "src" / "main" / "res" / "font";
    if (fs::exists(font_dir)) {
        for (const auto& entry : fs::directory_iterator(font_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf") {
                manifest.push_back({
                    "Font: " + entry.path().filename().string(),
                    entry.path(),
                    staging_dir / "res" / "font" / entry.path().filename(),
                    false
                });
            }
        }
    }

    return manifest;
}

int copy_essential_files(const std::vector<EssentialFile>& manifest,
                         bool verbose_copy) {
    int copied = 0;
    for (const auto& ef : manifest) {
        if (!fs::exists(ef.src)) {
            if (ef.required) {
                fatal("ASSET_COPY", err::CPY_001,
                      "Essential file missing: " + ef.category,
                      ef.src.string());
            } else {
                phase_warn("ASSET_COPY", "Optional essential file not found: " + ef.category
                           + "\n           " + ef.src.string());
                continue;
            }
        }
        copy_file_validated(ef.src, ef.dst, verbose_copy, true, "ASSET_COPY");
        ++copied;
    }
    return copied;
}

} // namespace kinetic
