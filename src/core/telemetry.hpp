#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  core/telemetry.hpp  —  Build report generation
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_timer.hpp"
#include <string>
#include <filesystem>

namespace kinetic {

namespace fs = std::filesystem;

struct TelemetrySummary {
    std::vector<PhaseRecord> phases;
    long long total_ns   = 0;
    bool      all_passed = true;
    fs::path  apk_path;
    uint64_t  apk_size   = 0;   // bytes
};

// Print the final KINETIC BUILD REPORT table to stdout.
void print_telemetry(const TelemetrySummary& summary);

// Build a TelemetrySummary from a completed PhaseTimer.
TelemetrySummary make_summary(const PhaseTimer& timer,
                              const fs::path& apk_path);

} // namespace kinetic
