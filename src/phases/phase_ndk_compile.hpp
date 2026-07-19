// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_ndk_compile.hpp  —  Phase 05: NDK_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseNdkCompile final : public Phase {
public:
    const char* name() const override { return "NDK_COMPILE"; }

    std::vector<std::string> requires() const override {
        return {"CMAKE_PARSE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        std::vector<fs::path> out;
        for (const auto& abi : ctx.cfg.abi_filters) {
            fs::path so = ctx.dirs.lib_dir / abi / "libkinetic.so";
            out.push_back(so);
        }
        return out;
    }

    bool parallelizable() const override { return true; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
