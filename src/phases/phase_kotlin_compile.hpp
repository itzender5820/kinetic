// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_kotlin_compile.hpp  —  Phase 07: KOTLIN_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/phase.hpp"

namespace kinetic {

class PhaseKotlinCompile final : public Phase {
public:
    const char* name() const override { return "KOTLIN_COMPILE"; }

    std::vector<std::string> requires() const override {
        return {"JAVA_COMPILE"};
    }

    std::vector<fs::path> provides(const PhaseContext& ctx) const override {
        std::vector<fs::path> out;
        if (fs::exists(ctx.dirs.classes_dir)) {
            for (const auto& e : fs::recursive_directory_iterator(ctx.dirs.classes_dir))
                if (e.is_regular_file() && e.path().extension() == ".class")
                    out.push_back(e.path());
        }
        return out;
    }

    bool parallelizable() const override { return true; }
    bool cacheable()      const override { return true; }

    void execute(PhaseContext& ctx) override;
};

} // namespace kinetic
