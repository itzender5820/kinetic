// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_so_copy.hpp  —  Phase 09: SO_COPY
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseSoCopy final : public Phase {
public:
    const char* name() const override { return "SO_COPY"; }

    std::vector<std::string> requires() const override {
        return {"NDK_COMPILE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        std::vector<fs::path> out;
        for (const auto& abi : ctx.cfg.abi_filters) {
            fs::path dst = ctx.dirs.staging_dir / "lib" / abi / "libkinetic.so";
            out.push_back(dst);
        }
        return out;
    }

    bool parallelizable() const override { return true; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
