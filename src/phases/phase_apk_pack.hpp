// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_apk_pack.hpp  —  Phase 12: APK_PACK
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseApkPack final : public Phase {
public:
    const char* name() const override { return "APK_PACK"; }

    std::vector<std::string> requires() const override {
        return {"LINK_APRS", "DEX_CONVERT", "SO_COPY", "ASSET_COPY", "MANIFEST_MERGE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        return {ctx.dirs.output_dir / (ctx.cfg.app_name + "-unsigned.apk")};
    }

    bool cacheable() const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
