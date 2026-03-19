// ─────────────────────────────────────────────────────────────────────────────
//  core/phase_timer.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_timer.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace kinetic {

void PhaseTimer::start(const std::string& phase_name) {
    current_phase_ = phase_name;
    start_time_    = Clock::now();
    running_       = true;
}

void PhaseTimer::stop(bool success, const std::string& output_desc) {
    if (!running_) return;
    auto end = Clock::now();
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       end - start_time_).count();
    records_.push_back({ current_phase_, ns, success, output_desc });
    running_ = false;
}

double PhaseTimer::elapsed_ms() const {
    if (!running_) return 0.0;
    auto now = Clock::now();
    return std::chrono::duration<double, std::milli>(now - start_time_).count();
}

std::string PhaseTimer::format_duration(long long ns) {
    std::ostringstream ss;
    if (ns < 1'000'000LL) {
        // Sub-millisecond: show μs
        ss << std::fixed << std::setprecision(0) << (ns / 1000.0) << " μs";
    } else if (ns < 1'000'000'000LL) {
        // Milliseconds
        long long ms = ns / 1'000'000LL;
        ss << ms << " ms";
    } else {
        // Seconds
        double s = ns / 1'000'000'000.0;
        ss << std::fixed << std::setprecision(2) << s << " s";
    }
    return ss.str();
}

const PhaseRecord* PhaseTimer::current() const {
    if (records_.empty()) return nullptr;
    return &records_.back();
}

} // namespace kinetic
