// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_sign_align.hpp  —  Phase 13: SIGN_ALIGN
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseSignAlign final : public Phase {
public:
    const char* name() const override { return "SIGN_ALIGN"; }

    std::vector<std::string> requires() const override {
        return {"APK_PACK"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        return {ctx.dirs.output_dir / (ctx.cfg.app_name + "-signed.apk")};
    }

    bool cacheable() const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
