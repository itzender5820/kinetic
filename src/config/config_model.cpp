// ─────────────────────────────────────────────────────────────────────────────
//  config_model.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "config_model.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include <stdexcept>
#include <algorithm>

namespace kinetic {

void KineticConfig::resolve_tilde_paths(const std::string& home) {
    auto expand = [&](std::string& p) {
        if (!p.empty() && p[0] == '~') {
            p = home + p.substr(1);
        }
    };
    expand(keystore);
    expand(java_sources);
    expand(kotlin_sources);
    expand(output_dir);
}

void KineticConfig::validate() const {
    if (package_name.empty()) {
        fatal("CMAKE_PARSE", err::CFG_003,
              "KINETIC_PACKAGE_NAME is required but not set",
              "kinetic.cmake",
              "Add: set(KINETIC_PACKAGE_NAME \"com.example.myapp\")");
    }
    if (min_sdk <= 0 || target_sdk <= 0) {
        fatal("CMAKE_PARSE", err::CFG_005,
              "KINETIC_MIN_SDK and KINETIC_TARGET_SDK must be positive integers",
              "kinetic.cmake");
    }
    if (target_sdk < min_sdk) {
        fatal("CMAKE_PARSE", err::CFG_005,
              "KINETIC_TARGET_SDK must be >= KINETIC_MIN_SDK",
              "kinetic.cmake");
    }

    static const std::vector<std::string> valid_abis = {
        "arm64-v8a", "armeabi-v7a", "x86_64", "x86"
    };
    for (const auto& abi : abi_filters) {
        if (std::find(valid_abis.begin(), valid_abis.end(), abi) == valid_abis.end()) {
            fatal("CMAKE_PARSE", err::CFG_004,
                  "Invalid ABI filter: '" + abi + "'",
                  "kinetic.cmake",
                  "Valid values: arm64-v8a, armeabi-v7a, x86_64, x86");
        }
    }
}

} // namespace kinetic
