// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_res_compile.hpp  —  Phase 03: RES_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseResCompile final : public Phase {
public:
    const char* name() const override { return "RES_COMPILE"; }

    std::vector<std::string> requires() const override {
        return {"CMAKE_PARSE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        std::vector<fs::path> out;
        if (fs::exists(ctx.dirs.flat_dir)) {
            for (const auto& e : fs::recursive_directory_iterator(ctx.dirs.flat_dir))
                if (e.is_regular_file() && e.path().extension() == ".flat")
                    out.push_back(e.path());
        }
        return out;
    }

    bool cacheable() const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
