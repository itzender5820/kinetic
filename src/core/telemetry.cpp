// ─────────────────────────────────────────────────────────────────────────────
//  core/telemetry.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "telemetry.hpp"
#include "phase_timer.hpp"
#include "../kinetic.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <numeric>
#include <cmath>

namespace kinetic {

// ── Formatting helpers ────────────────────────────────────────────────────────
static std::string pad_right(const std::string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    return s + std::string(width - s.size(), ' ');
}
static std::string pad_left(const std::string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    return std::string(width - s.size(), ' ') + s;
}

static std::string format_bytes(uint64_t bytes) {
    if (bytes == 0) return "?";
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024*1024) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
    return ss.str();
}

// ── print_telemetry ───────────────────────────────────────────────────────────
void print_telemetry(const TelemetrySummary& s) {
    // Column widths (fixed to match design doc)
    const int W_PHASE  = 15;
    const int W_DUR    = 8;
    const int W_STATUS = 6;
    const int W_OUTPUT = 19;
    const int TOTAL    = 3 + W_PHASE + 3 + W_DUR + 3 + W_STATUS + 3 + W_OUTPUT + 2;

    // Box drawing chars are multi-byte UTF-8; build separator strings with string literals
    auto rep_str = [](const std::string& s, int n) {
        std::string r; r.reserve(s.size() * n);
        for (int i = 0; i < n; ++i) r += s;
        return r;
    };
    std::string line  = rep_str("─", TOTAL);
    std::string thick = rep_str("═", TOTAL);

    auto box = [&](const std::string& s) {
        std::cout << c(col::DIM, "│") << " " << s << " " << c(col::DIM, "│") << "\n";
    };

    std::cout << "\n";
    std::cout << c(col::DIM, "┌" + thick + "┐") << "\n";

    // Title
    std::string title = "               KINETIC BUILD REPORT                ";
    title = pad_right(title, TOTAL);
    box(c(col::BOLD, title));

    std::cout << c(col::DIM, "├") << c(col::DIM, rep_str("─", W_PHASE+2))
              << c(col::DIM, "┬") << c(col::DIM, rep_str("─", W_DUR+2))
              << c(col::DIM, "┬") << c(col::DIM, rep_str("─", W_STATUS+2))
              << c(col::DIM, "┬") << c(col::DIM, rep_str("─", W_OUTPUT+2))
              << c(col::DIM, "┤") << "\n";

    // Header row
    auto header = " " + pad_right("Phase", W_PHASE)
                + " │ " + pad_left("Duration", W_DUR)
                + " │ " + pad_right("Status", W_STATUS)
                + " │ " + pad_right("Output", W_OUTPUT);
    box(c(col::BCYAN, header));

    std::cout << c(col::DIM, "├") << c(col::DIM, rep_str("─", W_PHASE+2))
              << c(col::DIM, "┼") << c(col::DIM, rep_str("─", W_DUR+2))
              << c(col::DIM, "┼") << c(col::DIM, rep_str("─", W_STATUS+2))
              << c(col::DIM, "┼") << c(col::DIM, rep_str("─", W_OUTPUT+2))
              << c(col::DIM, "┤") << "\n";

    // Phase rows
    for (const auto& p : s.phases) {
        std::string dur  = PhaseTimer::format_duration(p.duration_ns);
        std::string stat = p.success ? c(col::BGREEN, "  ✓   ") : c(col::BRED, "  ✗   ");
        std::string row  = " " + pad_right(p.name, W_PHASE)
                         + " │ " + c(col::DIM, pad_left(dur, W_DUR))
                         + " │ " + stat
                         + " │ " + pad_right(p.output_desc, W_OUTPUT);
        box(row);
    }

    std::cout << c(col::DIM, "├") << c(col::DIM, rep_str("─", TOTAL)) << c(col::DIM, "┤") << "\n";

    // Totals row
    std::string total_time = PhaseTimer::format_duration(s.total_ns);
    std::string apk_sz     = format_bytes(s.apk_size);
    std::string totals     = "  TOTAL BUILD TIME: " + pad_right(total_time, 12)
                           + "   APK: " + apk_sz;
    totals = pad_right(totals, TOTAL);
    box(c(col::BOLD, totals));

    // APK path
    std::string apk_line = "  Output: " + s.apk_path.string();
    apk_line = pad_right(apk_line, TOTAL);
    box(c(col::GREEN, apk_line));

    std::cout << c(col::DIM, "└" + thick + "┘") << "\n\n";
}

// ── make_summary ──────────────────────────────────────────────────────────────
TelemetrySummary make_summary(const PhaseTimer& timer, const fs::path& apk_path) {
    TelemetrySummary s;
    s.phases = timer.records();
    s.total_ns = 0;
    s.all_passed = true;
    for (const auto& r : s.phases) {
        s.total_ns += r.duration_ns;
        if (!r.success) s.all_passed = false;
    }
    s.apk_path = apk_path;
    if (fs::exists(apk_path)) {
        s.apk_size = (uint64_t)fs::file_size(apk_path);
    }
    return s;
}

} // namespace kinetic
