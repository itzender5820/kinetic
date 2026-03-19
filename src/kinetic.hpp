#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  kinetic.hpp  —  Master include for the Kinetic build system
//  All translation units include this to get the common types.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <optional>
#include <functional>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <csignal>

namespace fs = std::filesystem;

// ── ANSI colour helpers ───────────────────────────────────────────────────────
namespace col {
    inline constexpr const char* RESET   = "\033[0m";
    inline constexpr const char* BOLD    = "\033[1m";
    inline constexpr const char* DIM     = "\033[2m";
    inline constexpr const char* RED     = "\033[31m";
    inline constexpr const char* GREEN   = "\033[32m";
    inline constexpr const char* YELLOW  = "\033[33m";
    inline constexpr const char* BLUE    = "\033[34m";
    inline constexpr const char* MAGENTA = "\033[35m";
    inline constexpr const char* CYAN    = "\033[36m";
    inline constexpr const char* WHITE   = "\033[37m";
    inline constexpr const char* BGREEN  = "\033[1;32m";
    inline constexpr const char* BRED    = "\033[1;31m";
    inline constexpr const char* BYELLOW = "\033[1;33m";
    inline constexpr const char* BCYAN   = "\033[1;36m";
}

// Global colour toggle (set from CLI --no-color)
inline bool g_color_enabled = true;

inline std::string c(const char* ansi, const std::string& text) {
    if (!g_color_enabled) return text;
    return std::string(ansi) + text + col::RESET;
}
