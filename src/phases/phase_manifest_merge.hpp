// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_manifest_merge.hpp  —  Phase 11: MANIFEST_MERGE
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseManifestMerge final : public Phase {
public:
    const char* name() const override { return "MANIFEST_MERGE"; }

    std::vector<std::string> requires() const override {
        return {"CMAKE_PARSE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        return {ctx.dirs.staging_dir / "AndroidManifest.xml"};
    }

    bool parallelizable() const override { return true; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
