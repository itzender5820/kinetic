#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  error_codes.hpp  —  All Kinetic error code constants
// ─────────────────────────────────────────────────────────────────────────────

namespace kinetic::err {

// ── Environment ───────────────────────────────────────────────────────────────
inline constexpr const char* ENV_001 = "ENV_001"; // SDK not found under $HOME
inline constexpr const char* ENV_002 = "ENV_002"; // NDK not found under $HOME
inline constexpr const char* ENV_003 = "ENV_003"; // build-tools directory missing
inline constexpr const char* ENV_004 = "ENV_004"; // no build-tools version found
inline constexpr const char* ENV_005 = "ENV_005"; // SDK AAPT2 binary not found
inline constexpr const char* ENV_006 = "ENV_006"; // System AAPT2 not found
inline constexpr const char* ENV_007 = "ENV_007"; // d8 not found in build-tools
inline constexpr const char* ENV_008 = "ENV_008"; // apksigner not found
inline constexpr const char* ENV_009 = "ENV_009"; // zipalign not found
inline constexpr const char* ENV_010 = "ENV_010"; // NDK toolchain not found

// ── Configuration ─────────────────────────────────────────────────────────────
inline constexpr const char* CFG_001 = "CFG_001"; // kinetic.cmake not found
inline constexpr const char* CFG_002 = "CFG_002"; // syntax error in kinetic.cmake
inline constexpr const char* CFG_003 = "CFG_003"; // required variable missing
inline constexpr const char* CFG_004 = "CFG_004"; // invalid ABI filter value
inline constexpr const char* CFG_005 = "CFG_005"; // invalid SDK/NDK version number
inline constexpr const char* CFG_006 = "CFG_006"; // output directory creation failed

// ── Resource Compile ──────────────────────────────────────────────────────────
inline constexpr const char* RES_001 = "RES_001"; // res/ directory not found
inline constexpr const char* RES_002 = "RES_002"; // aapt2 compile failed
inline constexpr const char* RES_003 = "RES_003"; // malformed XML resource
inline constexpr const char* RES_004 = "RES_004"; // duplicate resource ID

// ── AAPT2 Link ────────────────────────────────────────────────────────────────
inline constexpr const char* RES_010 = "RES_010"; // aapt2 link failed
inline constexpr const char* RES_011 = "RES_011"; // resources.aprs not produced

// ── NDK Compile ───────────────────────────────────────────────────────────────
inline constexpr const char* NDK_001 = "NDK_001"; // clang++ not found in toolchain
inline constexpr const char* NDK_042 = "NDK_042"; // undefined reference in native source
inline constexpr const char* NDK_002 = "NDK_002"; // source file not found
inline constexpr const char* NDK_003 = "NDK_003"; // compilation error (clang++ output)
inline constexpr const char* NDK_004 = "NDK_004"; // linker error
inline constexpr const char* NDK_005 = "NDK_005"; // missing include directory

// ── Java / Kotlin ─────────────────────────────────────────────────────────────
inline constexpr const char* JVM_001 = "JVM_001"; // javac not found on PATH
inline constexpr const char* JVM_002 = "JVM_002"; // javac compilation error
inline constexpr const char* JVM_003 = "JVM_003"; // kotlinc not found on PATH
inline constexpr const char* JVM_004 = "JVM_004"; // kotlinc compilation error

// ── Dex ───────────────────────────────────────────────────────────────────────
inline constexpr const char* DEX_001 = "DEX_001"; // d8 invocation failed
inline constexpr const char* DEX_002 = "DEX_002"; // no .class files to convert
inline constexpr const char* DEX_003 = "DEX_003"; // desugaring failure

// ── File Copy ─────────────────────────────────────────────────────────────────
inline constexpr const char* CPY_001 = "CPY_001"; // source file missing
inline constexpr const char* CPY_002 = "CPY_002"; // copy failed (permission/disk)
inline constexpr const char* CPY_003 = "CPY_003"; // SHA-256 mismatch after copy
inline constexpr const char* CPY_004 = "CPY_004"; // ABI mismatch in .so path
inline constexpr const char* CPY_005 = "CPY_005"; // assets/ directory missing

// ── Packaging ─────────────────────────────────────────────────────────────────
inline constexpr const char* PKG_001 = "PKG_001"; // aapt2 package failed
inline constexpr const char* PKG_002 = "PKG_002"; // AndroidManifest.xml missing
inline constexpr const char* PKG_003 = "PKG_003"; // manifest validation failed
inline constexpr const char* PKG_004 = "PKG_004"; // APK output not produced

// ── Signing ───────────────────────────────────────────────────────────────────
inline constexpr const char* SGN_001 = "SGN_001"; // keystore not found
inline constexpr const char* SGN_002 = "SGN_002"; // apksigner failed (wrong password?)
inline constexpr const char* SGN_003 = "SGN_003"; // zipalign failed
inline constexpr const char* SGN_004 = "SGN_004"; // certificate expired

} // namespace kinetic::err
