#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  core/phase_timer.hpp  —  Nanosecond-precision phase timing
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <chrono>
#include <vector>

namespace kinetic {

struct PhaseRecord {
    std::string name;
    long long   duration_ns = 0;
    bool        success     = false;
    std::string output_desc;    // e.g. "42 .flat files"
};

class PhaseTimer {
public:
    void start(const std::string& phase_name);

    // Stop current phase and record it.
    // output_desc: short description of what was produced.
    void stop(bool success, const std::string& output_desc = "");

    // Return elapsed ms for display
    double elapsed_ms() const;

    // Return formatted duration string, e.g. "214 ms" or "1.34 s"
    static std::string format_duration(long long ns);

    const std::vector<PhaseRecord>& records() const { return records_; }
    const PhaseRecord* current() const;

private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    std::string current_phase_;
    TimePoint   start_time_;
    bool        running_ = false;

    std::vector<PhaseRecord> records_;
};

} // namespace kinetic
