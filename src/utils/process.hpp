#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  utils/process.hpp  —  Cross-platform subprocess execution with I/O capture
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <filesystem>

namespace kinetic {

namespace fs = std::filesystem;

struct ProcResult {
    int exit_code = -1;
    std::string out;    // captured stdout
    std::string err;    // captured stderr
};

// Execute a command with separate stdout/stderr capture.
// argv[0] must be the executable path or name (resolved via PATH).
// cwd: optional working directory for the child process.
// Returns ProcResult; never throws.
ProcResult exec_proc(const std::vector<std::string>& argv,
                     const fs::path& cwd = "");

// Resolve the absolute path of an executable via PATH search.
// Returns empty string if not found.
std::string which(const std::string& name);

// Expand ~ at the start of a path using the HOME environment variable.
std::string expand_home(const std::string& path);

} // namespace kinetic
