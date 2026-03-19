// ─────────────────────────────────────────────────────────────────────────────
//  env/env_scanner.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "env_scanner.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unistd.h>         // readlink()
#include <sys/utsname.h>    // uname()

namespace kinetic {

// ── Self-location ─────────────────────────────────────────────────────────────
// Returns the directory that contains the running kinetic binary.
// Uses /proc/self/exe on Linux (always available on Android/Termux too).
static fs::path get_self_dir() {
    char buf[4096] = {};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    return fs::path(std::string(buf, (size_t)n)).parent_path();
}

// ── Architecture detection ────────────────────────────────────────────────────
// Returns true when the current host machine is aarch64 (ARM64).
// This covers Android/Termux arm64-v8a devices.
static bool is_aarch64() {
    struct utsname uts {};
    if (uname(&uts) != 0) return false;
    std::string machine(uts.machine);
    return machine == "aarch64" || machine == "arm64";
}

// ── Bundled AAPT2 ─────────────────────────────────────────────────────────────
// On aarch64 hosts, Kinetic ships its own aapt2 binary inside:
//   <kinetic_binary_dir>/assets/aapt2-aarch64
// This removes the need for `pkg install aapt2` on Termux.
static fs::path find_bundled_aapt2() {
    fs::path self_dir = get_self_dir();
    if (self_dir.empty()) return {};

    // Primary: assets/ next to the kinetic binary
    fs::path candidate = self_dir / "assets" / "aapt2-aarch64";
    if (fs::exists(candidate)) return candidate;

    // Also check one level up (e.g. if binary is in dist/, assets/ is at root)
    candidate = self_dir.parent_path() / "assets" / "aapt2-aarch64";
    if (fs::exists(candidate)) return candidate;

    return {};
}

// ── Resolve an aapt2 path override ───────────────────────────────────────────
// The user may pass either:
//   --aapt2 /usr/bin/aapt2            (explicit binary)
//   --aapt2 /data/data/com.termux/files/usr/bin   (directory)
// Both forms are handled: if the path is a directory, we look for aapt2 inside it.
static fs::path resolve_aapt2_override(const std::string& raw) {
    if (raw.empty()) return {};
    fs::path p = fs::path(expand_home(raw));

    if (fs::is_directory(p)) {
        // Directory passed — look for the binary inside
        fs::path candidate = p / "aapt2";
        if (fs::exists(candidate)) return candidate;
        // Also check aapt2-aarch64 in case someone points at our assets/
        candidate = p / "aapt2-aarch64";
        if (fs::exists(candidate)) return candidate;
        // Not found inside — return the directory so we give a clear error
        return p / "aapt2";
    }
    return p;
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string latest_version_dir(const fs::path& dir) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return {};

    std::vector<std::string> versions;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            versions.push_back(entry.path().filename().string());
        }
    }
    if (versions.empty()) return {};

    std::sort(versions.begin(), versions.end(), [](const std::string& a, const std::string& b) {
        auto split = [](const std::string& s) -> std::vector<int> {
            std::vector<int> v;
            std::istringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, '.')) {
                try { v.push_back(std::stoi(tok)); } catch (...) { v.push_back(0); }
            }
            return v;
        };
        auto va = split(a), vb = split(b);
        for (size_t i = 0; i < std::max(va.size(), vb.size()); ++i) {
            int x = i < va.size() ? va[i] : 0;
            int y = i < vb.size() ? vb[i] : 0;
            if (x != y) return x < y;
        }
        return false;
    });
    return versions.back();
}

static fs::path find_ndk_root(const fs::path& candidate) {
    if (!fs::exists(candidate)) return {};
    if (fs::exists(candidate / "toolchains" / "llvm")) return candidate;
    for (const auto& entry : fs::directory_iterator(candidate)) {
        if (entry.is_directory() && fs::exists(entry.path() / "toolchains" / "llvm")) {
            return entry.path();
        }
    }
    return {};
}

static fs::path find_sdk(const std::string& home) {
    const std::vector<fs::path> candidates = {
        fs::path(home) / "android-sdk",
        fs::path(home) / "Android" / "Sdk",
        fs::path(home) / "Android" / "sdk",
        // /opt/android-sdk omitted — prefer $HOME and $PREFIX based discovery
    };
    for (const auto& c : candidates) {
        if (fs::exists(c) && fs::is_directory(c / "build-tools")) return c;
    }
    return {};
}

static fs::path find_ndk(const std::string& home) {
    const std::vector<std::string> exact = { "android-ndk", "AndroidNDK", "ndk" };
    for (const auto& name : exact) {
        auto r = find_ndk_root(fs::path(home) / name);
        if (!r.empty()) return r;
    }
    if (fs::exists(home)) {
        std::vector<fs::path> matches;
        for (const auto& entry : fs::directory_iterator(home)) {
            if (!entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            if (name.find("android-ndk") == 0 || name.find("ndk-r") == 0) {
                auto r = find_ndk_root(entry.path());
                if (!r.empty()) matches.push_back(r);
            }
        }
        if (!matches.empty()) {
            std::sort(matches.begin(), matches.end(), [](const fs::path& a, const fs::path& b) {
                return a.filename().string() < b.filename().string();
            });
            return matches.back();
        }
    }
    auto sdk = find_sdk(home);
    if (!sdk.empty()) {
        auto ndk_in_sdk = find_ndk_root(sdk / "ndk");
        if (!ndk_in_sdk.empty()) return ndk_in_sdk;
    }
    return {};
}

// ── System AAPT2 resolution (aarch64 priority order) ─────────────────────────
//
//  Priority on aarch64 hosts:
//    1. Kinetic bundled binary  (assets/aapt2-aarch64)  — always works, no install needed
//    2. Termux installed        (/data/data/com.termux/files/usr/bin/aapt2)
//    3. PATH                    (aapt2 anywhere on PATH)
//
//  On non-aarch64 hosts (x86_64 dev machines):
//    The "system aapt2" for APK_PACK is the same SDK aapt2.
//    sys_aapt2 is set to sdk_aapt2 as a fallback.
static fs::path find_system_aapt2(bool on_aarch64) {
    if (on_aarch64) {
        // 1. Bundled binary (ships with kinetic)
        fs::path bundled = find_bundled_aapt2();
        if (!bundled.empty() && fs::exists(bundled)) return bundled;

        // 2. $PREFIX/bin/aapt2 — works for Termux and any prefix-style install
        //    Termux sets $PREFIX=/data/data/com.termux/files/usr
        //    Never hardcode the Termux path directly.
        const char* prefix_env = std::getenv("PREFIX");
        if (prefix_env) {
            fs::path prefix_aapt2 = fs::path(prefix_env) / "bin" / "aapt2";
            if (fs::exists(prefix_aapt2)) return prefix_aapt2;
        }
    }

    // 3. PATH (works on any arch/distro)
    auto w = which("aapt2");
    if (!w.empty()) return fs::path(w);

    return {};
}

// ── NDK toolchain ─────────────────────────────────────────────────────────────
static void resolve_ndk_toolchain(KineticEnv& env, int api_level) {
    fs::path llvm = env.ndk_path / "toolchains" / "llvm" / "prebuilt";
    if (!fs::exists(llvm)) {
        fatal("ENV_SCAN", err::ENV_010,
              "NDK toolchains/llvm/prebuilt not found", llvm.string());
    }
    fs::path prebuilt_host;
    for (const auto& e : fs::directory_iterator(llvm)) {
        if (e.is_directory()) { prebuilt_host = e.path(); break; }
    }
    if (prebuilt_host.empty()) {
        fatal("ENV_SCAN", err::ENV_010, "NDK prebuilt directory is empty", llvm.string());
    }
    fs::path bin = prebuilt_host / "bin";
    env.ndk_sysroot = prebuilt_host / "sysroot";

    std::string api_str = std::to_string(api_level);
    fs::path clangxx = bin / ("aarch64-linux-android" + api_str + "-clang++");
    if (!fs::exists(clangxx)) clangxx = bin / "aarch64-linux-android-clang++";
    if (!fs::exists(clangxx)) clangxx = bin / "clang++";
    if (!fs::exists(clangxx)) {
        fatal("ENV_SCAN", err::ENV_001,
              "NDK clang++ not found in " + bin.string(), bin.string());
    }
    env.ndk_clangxx   = clangxx;
    env.ndk_triple    = "aarch64-linux-android";
    env.api_level_str = api_str;
}

// ── Main discover ─────────────────────────────────────────────────────────────
KineticEnv discover_env(const std::string& sdk_override,
                        const std::string& ndk_override,
                        const std::string& aapt2_override,
                        const std::string& build_tools_ver_hint) {
    KineticEnv env;

    const char* home_c = std::getenv("HOME");
    std::string home = home_c ? home_c : "/root";

    // Detect host architecture once
    const bool on_aarch64 = is_aarch64();
    const std::string arch_str = on_aarch64 ? "aarch64" : "x86_64";

    phase_log("ENV_SCAN", "Scanning $HOME (" + home + ") for SDK/NDK ...");
    phase_log("ENV_SCAN", "Host arch: " + arch_str);

    // ── SDK ───────────────────────────────────────────────────────────────────
    if (!sdk_override.empty()) {
        env.sdk_path = fs::path(expand_home(sdk_override));
    } else {
        env.sdk_path = find_sdk(home);
    }
    if (env.sdk_path.empty() || !fs::exists(env.sdk_path)) {
        fatal("ENV_SCAN", err::ENV_001,
              "Android SDK not found under $HOME",
              home + "/android-sdk");
    }
    env.sdk_found = true;

    // ── Build tools ───────────────────────────────────────────────────────────
    fs::path bt_dir = env.sdk_path / "build-tools";
    if (!fs::exists(bt_dir)) {
        fatal("ENV_SCAN", err::ENV_003,
              "build-tools/ directory not found in SDK", bt_dir.string());
    }
    std::string ver = build_tools_ver_hint.empty()
                      ? latest_version_dir(bt_dir)
                      : build_tools_ver_hint;
    if (ver.empty()) {
        fatal("ENV_SCAN", err::ENV_004, "No build-tools version installed", bt_dir.string());
    }
    env.build_tools_ver  = ver;
    env.build_tools_path = bt_dir / ver;

    // ── SDK AAPT2 (for RES_COMPILE + LINK_APRS) ───────────────────────────────
    //
    // IMPORTANT: The aapt2 in build-tools/<ver>/aapt2 is an x86_64 binary.
    // On aarch64 (Termux/Android) executing it gives:
    //   "Syntax error: word unexpected (expecting ")")"
    // because the kernel cannot run an x86_64 ELF.
    //
    // Fix: on aarch64 hosts, use the bundled aapt2-aarch64 for ALL aapt2 phases
    // (RES_COMPILE, LINK_APRS, APK_PACK). On x86_64 dev machines the SDK binary works.
    fs::path sdk_aapt2_path = env.build_tools_path / "aapt2";
    if (!fs::exists(sdk_aapt2_path)) {
        fatal("ENV_SCAN", err::ENV_005,
              "SDK aapt2 not found in build-tools/" + ver, sdk_aapt2_path.string());
    }

    if (on_aarch64) {
        // SDK aapt2 is x86_64 — cannot execute on this device.
        // Find an aarch64-native aapt2 (bundled > Termux > PATH).
        fs::path native_aapt2 = find_system_aapt2(true);
        if (native_aapt2.empty() || !fs::exists(native_aapt2)) {
            fs::path bundled_hint = get_self_dir() / "assets" / "aapt2-aarch64";
            fatal("ENV_SCAN", err::ENV_005,
                  "No aarch64-compatible aapt2 found",
                  sdk_aapt2_path.string(),
                  "The SDK build-tools aapt2 is x86_64 and cannot run on this device.\n"
                  "         Kinetic needs an aarch64 aapt2 for all AAPT2 phases.\n"
                  "         Expected bundled binary at: " + bundled_hint.string() + "\n"
                  "         Or install via Termux:  pkg install aapt2");
        }
        // Use the native binary for ALL three aapt2 phases
        env.sdk_aapt2 = native_aapt2;
        phase_log("ENV_SCAN", "aarch64 host: native aapt2 → " + native_aapt2.string());
    } else {
        // x86_64 dev machine: SDK binary is the right arch
        env.sdk_aapt2 = sdk_aapt2_path;
    }

    // ── d8, apksigner, zipalign ───────────────────────────────────────────────
    fs::path d8_path = env.build_tools_path / "d8";
    if (!fs::exists(d8_path)) d8_path = env.build_tools_path / "dx";
    if (!fs::exists(d8_path))
        phase_warn("ENV_SCAN", "d8/dx not found in build-tools — DEX phase will fail if needed");
    else
        env.d8 = d8_path;

    fs::path apksigner_path = env.build_tools_path / "apksigner";
    if (!fs::exists(apksigner_path))
        phase_warn("ENV_SCAN", "apksigner not found — SIGN phase will fail");
    else
        env.apksigner = apksigner_path;

    fs::path zipalign_path = env.build_tools_path / "zipalign";
    if (!fs::exists(zipalign_path))
        phase_warn("ENV_SCAN", "zipalign not found — APK alignment will be skipped");
    else
        env.zipalign = zipalign_path;

    // ── System AAPT2 (for APK_PACK phase) ────────────────────────────────────
    //
    // On aarch64: sdk_aapt2 is already the native binary, so sys_aapt2 = sdk_aapt2.
    // --aapt2 override always wins for sys_aapt2 (APK_PACK phase specifically).
    if (!aapt2_override.empty()) {
        fs::path resolved = resolve_aapt2_override(aapt2_override);
        if (!fs::exists(resolved)) {
            fatal("ENV_SCAN", err::ENV_006,
                  "AAPT2 override path not found: " + resolved.string(),
                  resolved.string(),
                  "Pass the full path to the aapt2 binary, or the directory containing it.");
        }
        env.sys_aapt2 = resolved;
        phase_log("ENV_SCAN", "sys AAPT2 override → " + resolved.string());
    } else if (on_aarch64) {
        // sdk_aapt2 was already resolved to the native binary above
        env.sys_aapt2 = env.sdk_aapt2;
    } else {
        // x86_64: try PATH, then fall back to sdk_aapt2
        auto w = which("aapt2");
        env.sys_aapt2 = w.empty() ? env.sdk_aapt2 : fs::path(w);
    }

    // Final AAPT2 summary log line
    {
        std::string aapt2_src;
        fs::path bundled = find_bundled_aapt2();
        if (!bundled.empty() && env.sdk_aapt2 == bundled)
            aapt2_src = "bundled";
        else if (env.sdk_aapt2 == (env.build_tools_path / "aapt2"))
            aapt2_src = "sdk";
        else
            aapt2_src = "aarch64";
        phase_log("ENV_SCAN", "AAPT2 → sdk:" + ver
                  + "  |  sys:" + env.sys_aapt2.filename().string()
                  + " (" + aapt2_src + ")");
    }

    // ── NDK ───────────────────────────────────────────────────────────────────
    if (!ndk_override.empty()) {
        env.ndk_path = fs::path(expand_home(ndk_override));
    } else {
        env.ndk_path = find_ndk(home);
    }
    if (env.ndk_path.empty() || !fs::exists(env.ndk_path)) {
        fatal("ENV_SCAN", err::ENV_002,
              "Android NDK not found under $HOME",
              home + "/android-ndk");
    }
    env.ndk_found = true;

    resolve_ndk_toolchain(env, 21);

    env.javac_path   = which("javac");
    env.kotlinc_path = which("kotlinc");

    // ── Kotlin stdlib discovery ───────────────────────────────────────────────
    // kotlin-stdlib.jar lives in <kotlinc_home>/lib/kotlin-stdlib.jar.
    // Resolve kotlinc binary → follow symlinks → go up two dirs → lib/.
    // Typical paths:
    //   Termux: /data/data/com.termux/files/usr/lib/kotlinc/lib/kotlin-stdlib.jar
    //   Linux:  /usr/share/kotlin/lib/kotlin-stdlib.jar  or
    //           /opt/kotlinc/lib/kotlin-stdlib.jar
    if (!env.kotlinc_path.empty()) {
        // Resolve symlink chain to get the real binary path
        char resolved[4096]{};
        fs::path kc = env.kotlinc_path;
        // Try readlink a few levels (kotlinc is often a shell script wrapper)
        // Walk up: bin/ → parent → lib/kotlin-stdlib.jar
        for (int depth = 0; depth < 4; ++depth) {
            fs::path lib_dir = kc.parent_path().parent_path() / "lib";
            fs::path stdlib  = lib_dir / "kotlin-stdlib.jar";
            if (fs::exists(stdlib)) {
                env.kotlin_stdlib = stdlib;
                break;
            }
            // Follow one level of symlink
            ssize_t n = readlink(kc.c_str(), resolved, sizeof(resolved) - 1);
            if (n <= 0) break;
            resolved[n] = 0;
            fs::path target = fs::path(std::string(resolved, n));
            if (!target.is_absolute()) target = kc.parent_path() / target;
            if (target == kc) break;
            kc = target;
        }
        // Fallback: search common install locations
        if (env.kotlin_stdlib.empty()) {
            // Build candidate list dynamically from environment — no hardcoded paths.
            std::vector<fs::path> candidates;

            // $PREFIX/lib/kotlinc/lib/kotlin-stdlib.jar  (Termux, any prefix install)
            const char* prefix_env = std::getenv("PREFIX");
            if (prefix_env)
                candidates.push_back(
                    fs::path(prefix_env) / "lib" / "kotlinc" / "lib" / "kotlin-stdlib.jar");

            // $HOME/kotlinc/lib/kotlin-stdlib.jar  (manual user install)
            candidates.push_back(fs::path(home) / "kotlinc" / "lib" / "kotlin-stdlib.jar");

            // Standard Unix share paths
            candidates.push_back(fs::path("/usr/share/kotlin/lib/kotlin-stdlib.jar"));
            candidates.push_back(fs::path("/usr/local/share/kotlin/lib/kotlin-stdlib.jar"));

            // $JAVA_HOME/../lib/kotlin-stdlib.jar (some distro layouts)
            const char* java_home = std::getenv("JAVA_HOME");
            if (java_home)
                candidates.push_back(
                    fs::path(java_home).parent_path() / "lib" / "kotlin-stdlib.jar");

            for (const auto& candidate : candidates) {
                if (fs::exists(candidate)) {
                    env.kotlin_stdlib = candidate;
                    break;
                }
            }
        }
        if (!env.kotlin_stdlib.empty()) {
            phase_log("ENV_SCAN", "kotlin-stdlib → " + env.kotlin_stdlib.string());
        } else {
            phase_warn("ENV_SCAN",
                "kotlin-stdlib.jar not found — Kotlin runtime will be missing from APK\n"
                "         App will crash with NoClassDefFoundError on device.");
        }
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    phase_log("ENV_SCAN", "SDK   → " + env.sdk_path.string());
    phase_log("ENV_SCAN", "NDK   → " + env.ndk_path.string());

    return env;
}

// ── print_env ─────────────────────────────────────────────────────────────────
void print_env(const KineticEnv& env) {
    auto row = [](const std::string& key, const std::string& val) {
        std::cout << "  " << c(col::CYAN, key) << "  " << val << "\n";
    };

    fs::path bundled = find_bundled_aapt2();
    std::string bundled_str = bundled.empty() ? "(not found)" : bundled.string();

    std::cout << "\n" << c(col::BOLD, "  ⚡ KINETIC — Environment Report") << "\n";
    std::string sep; for (int i = 0; i < 50; ++i) sep += "─";
    std::cout << "  " << sep << "\n";

    row("Host arch       ", is_aarch64() ? "aarch64 (ARM64)" : "x86_64");
    row("Kinetic binary  ", get_self_dir().string());
    row("Bundled aapt2   ", bundled_str);
    row("SDK Path        ", env.sdk_path.string());
    row("NDK Path        ", env.ndk_path.string());
    row("Build-Tools     ", env.build_tools_ver);
    row("SDK AAPT2       ", env.sdk_aapt2.string());
    row("System AAPT2    ", env.sys_aapt2.empty() ? "(not found)" : env.sys_aapt2.string());
    row("d8              ", env.d8.empty()         ? "(not found)" : env.d8.string());
    row("apksigner       ", env.apksigner.empty()  ? "(not found)" : env.apksigner.string());
    row("zipalign        ", env.zipalign.empty()   ? "(not found)" : env.zipalign.string());
    row("NDK clang++     ", env.ndk_clangxx.string());
    row("NDK sysroot     ", env.ndk_sysroot.string());
    row("javac           ", env.javac_path.empty()   ? "(not on PATH)" : env.javac_path);
    row("kotlinc         ", env.kotlinc_path.empty() ? "(not on PATH)" : env.kotlinc_path);
    std::cout << "\n";
}

} // namespace kinetic
