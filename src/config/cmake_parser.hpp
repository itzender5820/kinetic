#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  cmake_parser.hpp  —  Kinetic's built-in CMake variable parser
//
//  Supports:
//    set(VAR value)
//    set(VAR val1 val2 val3)        — produces a list
//    set(VAR "quoted value")
//    list(APPEND VAR val1 val2)
//    # comments
//    if() / endif()                  — basic, just skipped gracefully
// ─────────────────────────────────────────────────────────────────────────────
#include "config_model.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace kinetic {

namespace fs = std::filesystem;

class CMakeParser {
public:
    // Parse the given kinetic.cmake file and return a populated KineticConfig.
    // project_root is the directory containing kinetic.cmake.
    KineticConfig parse(const fs::path& cmake_file, const fs::path& project_root);

private:
    // Raw variable store: each variable maps to a list of string tokens.
    std::unordered_map<std::string, std::vector<std::string>> vars_;

    // Parse a single logical line (after continuation joining) into vars_
    void process_command(const std::string& cmd_name,
                         const std::vector<std::string>& args,
                         const fs::path& file,
                         int line_no);

    // Tokenise inside parentheses, respecting quotes
    std::vector<std::string> tokenise_args(const std::string& raw) const;

    // Dereference ${VAR} references in a string
    std::string expand_var(const std::string& token) const;

    // Scalar helpers
    std::string str(const std::string& var, const std::string& def = "") const;
    int         integer(const std::string& var, int def = 0) const;
    bool        boolean(const std::string& var, bool def = false) const;
    std::vector<std::string> list(const std::string& var) const;

    // Map parsed vars_ into a KineticConfig
    KineticConfig build_config(const fs::path& project_root) const;
};

} // namespace kinetic
