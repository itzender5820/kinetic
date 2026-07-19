// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_env_scan.hpp  —  Phase 01: ENV_SCAN
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseEnvScan final : public Phase {
public:
    const char* name() const override { return "ENV_SCAN"; }

    std::vector<std::string> requires() const override { return {}; }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        // ENV_SCAN produces no on-disk artifacts; it populates ctx.env.
        return {};
    }

    bool parallelizable() const override { return false; }
    bool cacheable()     const override { return false; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
