// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_dex_convert.hpp  —  Phase 08: DEX_CONVERT
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseDexConvert final : public Phase {
public:
    const char* name() const override { return "DEX_CONVERT"; }

    std::vector<std::string> requires() const override {
        return {"JAVA_COMPILE", "KOTLIN_COMPILE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        return {ctx.dirs.dex_dir / "classes.dex"};
    }

    bool parallelizable() const override { return false; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
