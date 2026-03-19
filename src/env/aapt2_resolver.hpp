#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  env/aapt2_resolver.hpp  —  Dual AAPT2 resolution helpers
// ─────────────────────────────────────────────────────────────────────────────
#include "env_scanner.hpp"
#include "../config/config_model.hpp"

namespace kinetic {

// Re-resolve NDK toolchain after config is parsed (to get the correct API level).
// Mutates env.ndk_clangxx and env.api_level_str.
void reresolve_ndk_for_api(KineticEnv& env, int min_sdk);

} // namespace kinetic
