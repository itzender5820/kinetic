// ─────────────────────────────────────────────────────────────────────────────
//  error_reporter.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "error_reporter.hpp"
#include "hints.hpp"
#include "../kinetic.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace kinetic {

// Pad/truncate a phase label to a fixed width for alignment
static std::string pad_phase(const std::string& phase, int width = 12) {
    std::string s = phase;
    if ((int)s.size() > width) s = s.substr(0, width);
    while ((int)s.size() < width) s += ' ';
    return s;
}

void report_error(const ErrorInfo& e) {
    std::string hint_text = e.hint.empty() ? get_hint(e.code) : e.hint;

    std::ostream& out = std::cerr;
    out << "\n";
    out << c(col::BRED, "✗ [" + e.phase + "] " + e.code + ": " + e.short_desc) << "\n";

    if (!e.file.empty()) {
        out << c(col::DIM, "  File   : ") << e.file << "\n";
    }
    if (e.line >= 0) {
        out << c(col::DIM, "  Line   : ") << e.line;
        if (e.col >= 0) out << "  Col: " << e.col;
        out << "\n";
    }
    if (!e.detail.empty()) {
        // Indent multi-line detail
        std::istringstream ss(e.detail);
        std::string line;
        bool first = true;
        while (std::getline(ss, line)) {
            if (first) {
                out << c(col::DIM, "  Detail : ") << line << "\n";
                first = false;
            } else {
                out << "           " << line << "\n";
            }
        }
    }
    if (!hint_text.empty()) {
        out << c(col::YELLOW, "  Hint   : ") << hint_text << "\n";
    }
    out << c(col::DIM, "  Docs   : ") << "https://kinetic.dev/errors/" << e.code << "\n";
    out << "\n";
}

[[noreturn]] void fatal(const ErrorInfo& e) {
    report_error(e);
    throw std::runtime_error(e.code + ": " + e.short_desc);
}

[[noreturn]] void fatal(const std::string& phase,
                        const std::string& code,
                        const std::string& short_desc,
                        const std::string& file,
                        const std::string& detail) {
    fatal(ErrorInfo{ phase, code, short_desc, file, -1, -1, detail, "" });
}

void phase_log(const std::string& phase, const std::string& msg) {
    std::string label = "[" + pad_phase(phase) + "]";
    std::cout << c(col::CYAN, label) << "  " << msg << "\n";
}

void phase_warn(const std::string& phase, const std::string& msg) {
    std::string label = "[" + pad_phase(phase) + "]";
    std::cout << c(col::BYELLOW, label) << "  " << c(col::YELLOW, "WARN ") << msg << "\n";
}

} // namespace kinetic
