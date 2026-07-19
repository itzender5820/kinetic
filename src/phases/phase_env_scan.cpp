// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_env_scan.cpp  —  Phase 01: ENV_SCAN
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_env_scan.hpp"
#include "../env/env_scanner.hpp"

namespace kinetic {

void PhaseEnvScan::execute(PhaseContext& ctx) {
    ctx.env = discover_env(ctx.sdk_override,
                           ctx.ndk_override,
                           ctx.aapt2_override);
}

} // namespace kinetic
