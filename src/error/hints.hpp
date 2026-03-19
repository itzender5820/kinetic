#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  hints.hpp  —  Actionable fix suggestions keyed by error code
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <unordered_map>

namespace kinetic {

inline const std::unordered_map<std::string, std::string>& get_hints() {
    static const std::unordered_map<std::string, std::string> hints = {
        // ENV
        { "ENV_001", "Place android-sdk/ directly in $HOME, or use --sdk <path> to override." },
        { "ENV_002", "Place android-ndk/ (or android-ndk-r26c, etc.) directly in $HOME, or use --ndk <path>." },
        { "ENV_003", "Run: sdkmanager 'build-tools;34.0.0' to install build tools." },
        { "ENV_004", "Install build-tools via sdkmanager: sdkmanager 'build-tools;34.0.0'" },
        { "ENV_005", "Reinstall build-tools or check that aapt2 exists in build-tools/<ver>/aapt2." },
        { "ENV_006", "Install via Termux: pkg install aapt2   or use --aapt2 <path> to override." },
        { "ENV_007", "Install build-tools 28+: sdkmanager 'build-tools;34.0.0'  (d8 requires 28+)." },
        { "ENV_008", "Install build-tools 30+: sdkmanager 'build-tools;34.0.0'  (apksigner requires 30+)." },
        { "ENV_009", "Install build-tools: sdkmanager 'build-tools;34.0.0'" },
        { "ENV_010", "Check NDK toolchains/llvm/prebuilt/ directory exists and is not corrupted." },
        // CFG
        { "CFG_001", "Create kinetic.cmake in your project root. See 'kinetic --help' for reference." },
        { "CFG_002", "Fix the syntax error in kinetic.cmake. Use 'set(KINETIC_VAR value)' format." },
        { "CFG_003", "Add the missing KINETIC_ variable to kinetic.cmake. See --help for full reference." },
        { "CFG_004", "Valid ABI filters: arm64-v8a, armeabi-v7a, x86_64, x86." },
        { "CFG_005", "Ensure KINETIC_MIN_SDK and KINETIC_TARGET_SDK are positive integers." },
        { "CFG_006", "Check disk space and permissions for the output directory." },
        // RES
        { "RES_001", "Create src/main/res/ directory with at least values/strings.xml." },
        { "RES_002", "Fix the XML error in the indicated resource file and re-run." },
        { "RES_003", "Validate your XML with: xmllint --noout <file>" },
        { "RES_004", "Remove the duplicate resource definition from the indicated file." },
        { "RES_010", "Check AndroidManifest.xml is valid and all resource IDs are defined." },
        { "RES_011", "aapt2 link failed to produce resources.aprs — check aapt2 stderr above." },
        // NDK
        { "NDK_001", "Check NDK path is valid: kinetic --env" },
        { "NDK_042", "Add the missing source file to KINETIC_SOURCES in kinetic.cmake." },
        { "NDK_002", "Verify the source file path in KINETIC_SOURCES matches the actual file location." },
        { "NDK_003", "Fix the compilation error shown above. Check includes and API level compatibility." },
        { "NDK_004", "Add the missing library to KINETIC_LINK_LIBS in kinetic.cmake." },
        { "NDK_005", "Add the include path to KINETIC_INCLUDE_DIRS in kinetic.cmake." },
        // JVM
        { "JVM_001", "Install Java: apt install openjdk-17-jdk  or ensure javac is on PATH." },
        { "JVM_002", "Fix the Java compilation error shown above." },
        { "JVM_003", "Install Kotlin: snap install kotlin  or download from kotlinlang.org." },
        { "JVM_004", "Fix the Kotlin compilation error shown above." },
        // DEX
        { "DEX_001", "Check d8 is present: kinetic --env" },
        { "DEX_002", "Ensure Java/Kotlin sources compiled successfully before dex conversion." },
        { "DEX_003", "Lower KINETIC_MIN_SDK or add desugaring dependencies." },
        // CPY
        { "CPY_001", "Ensure the file exists at the indicated path before building." },
        { "CPY_002", "Check disk space (df -h) and directory permissions." },
        { "CPY_003", "The copied file is corrupt. Check disk health and free space." },
        { "CPY_004", "Place .so files in libs/<abi>/ matching KINETIC_ABI_FILTERS." },
        { "CPY_005", "Create src/main/assets/ directory (can be empty)." },
        // PKG
        { "PKG_001", "Check aapt2 (aarch64) is installed: pkg install aapt2" },
        { "PKG_002", "Create src/main/AndroidManifest.xml with a valid manifest element." },
        { "PKG_003", "Ensure AndroidManifest.xml has <manifest> root with package attribute." },
        { "PKG_004", "Check aapt2 output above for packaging errors." },
        // SGN
        { "SGN_001", "Generate a debug keystore: keytool -genkeypair -v -keystore ~/.android/debug.keystore -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -storepass android -keypass android" },
        { "SGN_002", "Check KINETIC_KEY_PASSWORD and KINETIC_STORE_PASSWORD in kinetic.cmake." },
        { "SGN_003", "Check zipalign is installed: kinetic --env" },
        { "SGN_004", "Regenerate your signing certificate — it has expired." },
    };
    return hints;
}

inline std::string get_hint(const std::string& code) {
    const auto& h = get_hints();
    auto it = h.find(code);
    if (it != h.end()) return it->second;
    return "See https://kinetic.dev/errors/" + code;
}

} // namespace kinetic
