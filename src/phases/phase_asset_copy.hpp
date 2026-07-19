// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_asset_copy.hpp  —  Phase 10: ASSET_COPY
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseAssetCopy final : public Phase {
public:
    const char* name() const override { return "ASSET_COPY"; }

    std::vector<std::string> requires() const override {
        return {"CMAKE_PARSE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        return {ctx.dirs.staging_dir / "assets" / "kinetic"};
    }

    bool parallelizable() const override { return true; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
