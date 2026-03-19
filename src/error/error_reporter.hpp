#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  error_reporter.hpp  —  Phase-tagged structured error output
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <optional>

namespace kinetic {

struct ErrorInfo {
    std::string phase;          // e.g. "NDK_COMPILE"
    std::string code;           // e.g. "NDK_042"
    std::string short_desc;     // One-line description
    std::string file;           // Optional: offending file
    int line   = -1;            // Optional: line number
    int col    = -1;            // Optional: column number
    std::string detail;         // Extended explanation
    std::string hint;           // Overrides the default hint if non-empty
};

// Print a structured, ANSI-coloured error block to stderr.
void report_error(const ErrorInfo& e);

// Fatal: print error and throw std::runtime_error with the code.
[[noreturn]] void fatal(const ErrorInfo& e);

// Quick fatal with just phase + code + short desc (hint comes from hints.hpp)
[[noreturn]] void fatal(const std::string& phase,
                        const std::string& code,
                        const std::string& short_desc,
                        const std::string& file   = "",
                        const std::string& detail = "");

// Print a phase-prefixed info line to stdout.
void phase_log(const std::string& phase, const std::string& msg);

// Print a phase-prefixed warning (yellow) to stdout.
void phase_warn(const std::string& phase, const std::string& msg);

} // namespace kinetic
