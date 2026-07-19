// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_cmake_parse.hpp  —  Phase 02: CMAKE_PARSE
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseCmakeParse final : public Phase {
public:
    const char* name() const override { return "CMAKE_PARSE"; }

    std::vector<std::string> requires() const override { return {"ENV_SCAN"}; }

    std::vector<fs::path> provides(const PhaseContext&) const override {
        return {};
    }

    bool parallelizable() const override { return false; }
    bool cacheable()     const override { return false; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
