// ─────────────────────────────────────────────────────────────────────────────
//  cmake_parser.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "cmake_parser.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../kinetic.hpp"

#include <fstream>
#include <sstream>
#include <cctype>
#include <cassert>
#include <regex>

namespace kinetic {

// ── Tokeniser ─────────────────────────────────────────────────────────────────
// Splits the raw content between the outer parentheses into tokens.
// Supports quoted strings and semicolon-separated lists.
std::vector<std::string> CMakeParser::tokenise_args(const std::string& raw) const {
    std::vector<std::string> tokens;
    std::string cur;
    bool in_quote = false;
    bool escaped  = false;

    for (size_t i = 0; i < raw.size(); ++i) {
        char ch = raw[i];

        if (escaped) {
            cur += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_quote = !in_quote;
            continue;
        }
        if (in_quote) {
            cur += ch;
            continue;
        }
        // Outside quotes: whitespace or semicolon are delimiters
        if (std::isspace((unsigned char)ch) || ch == ';') {
            if (!cur.empty()) {
                tokens.push_back(expand_var(cur));
                cur.clear();
            }
            continue;
        }
        cur += ch;
    }
    if (!cur.empty()) tokens.push_back(expand_var(cur));
    return tokens;
}

std::string CMakeParser::expand_var(const std::string& token) const {
    // Replace ${VAR_NAME} references
    std::string result;
    size_t i = 0;
    while (i < token.size()) {
        if (token[i] == '$' && i + 1 < token.size() && token[i+1] == '{') {
            size_t end = token.find('}', i + 2);
            if (end != std::string::npos) {
                std::string var_name = token.substr(i + 2, end - i - 2);
                auto it = vars_.find(var_name);
                if (it != vars_.end() && !it->second.empty()) {
                    result += it->second[0]; // use first value
                }
                i = end + 1;
                continue;
            }
        }
        result += token[i++];
    }
    return result;
}

// ── Command processor ─────────────────────────────────────────────────────────
void CMakeParser::process_command(const std::string& cmd,
                                  const std::vector<std::string>& args,
                                  const fs::path& file,
                                  int line_no) {
    std::string cmd_upper = cmd;
    for (char& c : cmd_upper) c = (char)std::toupper((unsigned char)c);

    if (cmd_upper == "SET") {
        if (args.empty()) return; // malformed but silent
        const std::string& var_name = args[0];
        // Remaining args are the value(s)
        std::vector<std::string> vals(args.begin() + 1, args.end());
        vars_[var_name] = vals;
    }
    else if (cmd_upper == "LIST") {
        if (args.size() < 2) return;
        std::string sub = args[0];
        for (char& c : sub) c = (char)std::toupper((unsigned char)c);
        const std::string& var_name = args[1];

        if (sub == "APPEND") {
            auto& lst = vars_[var_name];
            for (size_t i = 2; i < args.size(); ++i)
                lst.push_back(args[i]);
        }
        else if (sub == "REMOVE_ITEM") {
            auto& lst = vars_[var_name];
            for (size_t i = 2; i < args.size(); ++i) {
                lst.erase(std::remove(lst.begin(), lst.end(), args[i]), lst.end());
            }
        }
        // Other sub-commands are ignored
    }
    // if/else/endif/message/etc. are silently ignored
    (void)file; (void)line_no;
}

// ── Main parse ────────────────────────────────────────────────────────────────
KineticConfig CMakeParser::parse(const fs::path& cmake_file, const fs::path& project_root) {
    if (!fs::exists(cmake_file)) {
        fatal("CMAKE_PARSE", err::CFG_001,
              "kinetic.cmake not found",
              cmake_file.string(),
              "Create kinetic.cmake in your project root.\nRun: kinetic --help for a full reference.");
    }

    std::ifstream f(cmake_file);
    if (!f.is_open()) {
        fatal("CMAKE_PARSE", err::CFG_001,
              "Cannot open kinetic.cmake (permission denied?)",
              cmake_file.string());
    }

    phase_log("CMAKE_PARSE", "Reading " + cmake_file.filename().string() + " ...");

    std::string full_text;
    {
        std::ostringstream ss;
        ss << f.rdbuf();
        full_text = ss.str();
    }

    // ── Step 1: Strip comments, join continuation lines ──────────────────────
    // Process line by line
    std::vector<std::string> lines;
    {
        std::istringstream ss(full_text);
        std::string line;
        while (std::getline(ss, line)) {
            // Strip inline # comments (but not inside strings — simplified)
            // Find # not inside quotes
            bool in_q = false;
            for (size_t i = 0; i < line.size(); ++i) {
                if (line[i] == '"') in_q = !in_q;
                if (!in_q && line[i] == '#') {
                    line = line.substr(0, i);
                    break;
                }
            }
            // Trim trailing whitespace
            while (!line.empty() && std::isspace((unsigned char)line.back()))
                line.pop_back();
            lines.push_back(line);
        }
    }

    // ── Step 2: Join multi-line commands (find matching parentheses) ──────────
    // Kinetic cmake files are mostly single-line, but set() can span lines.
    std::vector<std::pair<int, std::string>> commands; // (line_no, full_command_text)
    {
        std::string accumulator;
        int start_line = 0;
        int depth = 0;

        for (int i = 0; i < (int)lines.size(); ++i) {
            const std::string& l = lines[i];
            if (accumulator.empty()) {
                // Skip blank / comment-only lines
                bool blank = true;
                for (char c : l) if (!std::isspace((unsigned char)c)) { blank = false; break; }
                if (blank) continue;
                accumulator = l;
                start_line = i + 1;
            } else {
                accumulator += " " + l;
            }
            // Count parens
            for (char c : l) {
                if (c == '(') ++depth;
                else if (c == ')') --depth;
            }
            if (depth <= 0) {
                commands.push_back({ start_line, accumulator });
                accumulator.clear();
                depth = 0;
            }
        }
        if (!accumulator.empty()) {
            // Unclosed paren — treat as incomplete command
            commands.push_back({ start_line, accumulator });
        }
    }

    // ── Step 3: Parse each command ────────────────────────────────────────────
    // Pattern: COMMAND_NAME ( args... )
    std::regex cmd_re(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*$)",
                      std::regex::optimize);

    int key_count = 0;
    for (auto& [line_no, cmd_text] : commands) {
        // For multi-line: find first '(' and last ')'
        size_t paren_open = cmd_text.find('(');
        size_t paren_close = cmd_text.rfind(')');
        if (paren_open == std::string::npos || paren_close == std::string::npos)
            continue;

        std::string cmd_name = cmd_text.substr(0, paren_open);
        // Trim cmd_name
        while (!cmd_name.empty() && std::isspace((unsigned char)cmd_name.back()))
            cmd_name.pop_back();
        while (!cmd_name.empty() && std::isspace((unsigned char)cmd_name.front()))
            cmd_name = cmd_name.substr(1);

        std::string args_raw = cmd_text.substr(paren_open + 1,
                                               paren_close - paren_open - 1);
        auto args = tokenise_args(args_raw);
        if (!cmd_name.empty()) {
            process_command(cmd_name, args, cmake_file, line_no);
            ++key_count;
        }
    }

    phase_log("CMAKE_PARSE", "Target: " + str("KINETIC_PACKAGE_NAME", "<unknown>")
              + "  min=" + std::to_string(integer("KINETIC_MIN_SDK", 21))
              + "  target=" + std::to_string(integer("KINETIC_TARGET_SDK", 34)));
    phase_log("CMAKE_PARSE", "Done  (" + std::to_string(key_count) + " config keys)");

    return build_config(project_root);
}

// ── Scalar helpers ────────────────────────────────────────────────────────────
std::string CMakeParser::str(const std::string& var, const std::string& def) const {
    auto it = vars_.find(var);
    if (it == vars_.end() || it->second.empty()) return def;
    return it->second[0];
}

int CMakeParser::integer(const std::string& var, int def) const {
    auto s = str(var, "");
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

bool CMakeParser::boolean(const std::string& var, bool def) const {
    auto s = str(var, "");
    if (s.empty()) return def;
    // CMake: ON/TRUE/YES/1 → true, OFF/FALSE/NO/0 → false
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return (s == "ON" || s == "TRUE" || s == "YES" || s == "1");
}

std::vector<std::string> CMakeParser::list(const std::string& var) const {
    auto it = vars_.find(var);
    if (it == vars_.end()) return {};
    return it->second;
}

// ── Map to KineticConfig ──────────────────────────────────────────────────────
KineticConfig CMakeParser::build_config(const fs::path& project_root) const {
    KineticConfig cfg;
    cfg.project_root    = project_root.string();

    cfg.app_name        = str("KINETIC_APP_NAME",      "MyApplication");
    cfg.package_name    = str("KINETIC_PACKAGE_NAME",  "");
    cfg.version_code    = integer("KINETIC_VERSION_CODE", 1);
    cfg.version_name    = str("KINETIC_VERSION_NAME",  "1.0.0");

    cfg.min_sdk         = integer("KINETIC_MIN_SDK",       21);
    cfg.target_sdk      = integer("KINETIC_TARGET_SDK",    34);
    cfg.compile_sdk     = integer("KINETIC_COMPILE_SDK",   34);
    cfg.build_tools_ver = str("KINETIC_BUILD_TOOLS_VER", "");

    auto abi_list = list("KINETIC_ABI_FILTERS");
    if (!abi_list.empty()) cfg.abi_filters = abi_list;

    cfg.cpp_standard    = integer("KINETIC_CPP_STANDARD", 17);
    cfg.cpp_flags       = str("KINETIC_CPP_FLAGS",    "-O2 -Wall -Wextra");
    cfg.java_version    = str("KINETIC_JAVA_VERSION", "11");
    cfg.kotlin_version  = str("KINETIC_KOTLIN_VERSION","1.9");

    cfg.sources         = list("KINETIC_SOURCES");
    cfg.include_dirs    = list("KINETIC_INCLUDE_DIRS");
    cfg.link_libs       = list("KINETIC_LINK_LIBS");
    cfg.prebuilt_libs   = list("KINETIC_PREBUILT_LIBS");

    cfg.java_sources    = str("KINETIC_JAVA_SOURCES",   "src/main/java");
    cfg.kotlin_sources  = str("KINETIC_KOTLIN_SOURCES", "src/main/kotlin");

    cfg.keystore        = str("KINETIC_KEYSTORE",       "~/.android/debug.keystore");
    cfg.key_alias       = str("KINETIC_KEY_ALIAS",      "androiddebugkey");
    cfg.key_password    = str("KINETIC_KEY_PASSWORD",   "android");
    cfg.store_password  = str("KINETIC_STORE_PASSWORD", "android");

    cfg.output_dir      = str("KINETIC_OUTPUT_DIR",  "build/outputs");
    cfg.output_name     = str("KINETIC_OUTPUT_NAME", "app-debug");

    cfg.telemetry       = boolean("KINETIC_TELEMETRY",     true);
    cfg.verbose_copy    = boolean("KINETIC_VERBOSE_COPY",  true);
    cfg.color_output    = boolean("KINETIC_COLOR_OUTPUT",  true);

    cfg.extra_assets    = list("KINETIC_EXTRA_ASSETS");

    return cfg;
}

} // namespace kinetic
