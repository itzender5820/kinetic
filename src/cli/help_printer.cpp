// ─────────────────────────────────────────────────────────────────────────────
//  cli/help_printer.cpp  —  ANSI-art help panel — pixel-perfect alignment
//
//  Box geometry (all measurements in visual terminal columns):
//    Total line width : 68  (║ + 66 interior + ║)
//    Interior (BOX_W): 66
//    2-col left (COL_L): 14   right (COL_R): 51  (66-14-1)
//    Examples left (EX_L): 37  right (EX_R): 29  (66-37)
//
//  Every ║-bordered row satisfies:  vlen(stripped_line) == 68
// ─────────────────────────────────────────────────────────────────────────────
#include "help_printer.hpp"
#include "../kinetic.hpp"
#include <iostream>
#include <string>

namespace kinetic {

// ── Visual-width counter (no ANSI codes inside strings passed to vlen) ────────
// Counts terminal columns: ASCII=1, 2-byte=1, 3-byte=1 (box-drawing),
// ⚡ U+26A1 (E2 9A A1) = 2 cols in Termux, 4-byte emoji = 2 cols.
static int vlen(const std::string& s) {
    int n = 0; size_t i = 0;
    const auto* b = reinterpret_cast<const unsigned char*>(s.data());
    const size_t sz = s.size();
    while (i < sz) {
        unsigned char c = b[i];
        if      (c < 0x80)           { n += 1; i += 1; }
        else if ((c & 0xE0) == 0xC0) { n += 1; i += 2; }
        else if ((c & 0xF0) == 0xE0) {
            if (i+2 < sz && b[i]==0xE2 && b[i+1]==0x9A && b[i+2]==0xA1)
                n += 2;   // ⚡ wide
            else
                n += 1;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0) { n += 2; i += 4; }
        else                          {          i += 1; }
    }
    return n;
}

// Pad string to exactly `w` visual columns with trailing spaces.
static std::string vpad(const std::string& s, int w) {
    int p = w - vlen(s);
    return p > 0 ? s + std::string(p, ' ') : s;
}

// Repeat a UTF-8 string n times (for box-drawing chars).
static std::string rep(const std::string& ch, int n) {
    std::string r; r.reserve(ch.size() * (size_t)n);
    for (int i = 0; i < n; ++i) r += ch;
    return r;
}

// ── Box geometry ──────────────────────────────────────────────────────────────
static const int BOX_W = 66;               // interior visual width
static const int COL_L = 14;              // standard 2-col left width
static const int COL_R = BOX_W-COL_L-1;  // = 51
static const int EX_L  = 37;             // examples left col (fits longest cmd)
static const int EX_R  = BOX_W-EX_L;    // = 29

// ── Row emitters ──────────────────────────────────────────────────────────────

// Full-width: ║<content padded to BOX_W>║
static void row(const std::string& txt, const char* color,
                const char* B, const char* R) {
    std::cout << B << "║" << R
              << color << vpad(txt, BOX_W) << R
              << B << "║\n" << R;
}

// Standard 2-column: ║<COL_L>║<COL_R>║
static void row2(const std::string& left,  const char* lc,
                 const std::string& right, const char* rc,
                 const char* B, const char* R) {
    std::cout << B << "║" << R
              << lc << vpad(left,  COL_L) << R
              << B  << "║" << R
              << rc << vpad(right, COL_R) << R
              << B  << "║\n" << R;
}

// Examples 2-column (wider left): ║<EX_L>║<EX_R>║  — NO middle ║
// Uses a single-column full-width row so colours don't create a fake border.
static void ex_row(const std::string& cmd,  const char* cc,
                   const std::string& desc, const char* dc,
                   const char* B, const char* R) {
    // Render as: ║ <cmd colour> <padded cmd> R <desc colour> <padded desc> R B ║
    std::cout << B << "║" << R
              << cc << vpad(cmd,  EX_L) << R
              << dc << vpad(desc, EX_R) << R
              << B  << "║\n" << R;
}

// Horizontal rules
static void hr (const char* B, const char* R) {
    std::cout << B << "╠" << rep("═", BOX_W)          << "╣\n" << R;
}
static void hr2(const char* B, const char* R) {   // open 2-col
    std::cout << B << "╠" << rep("═",COL_L) << "╦" << rep("═",COL_R) << "╣\n" << R;
}
static void hr2c(const char* B, const char* R) {  // close 2-col
    std::cout << B << "╠" << rep("═",COL_L) << "╩" << rep("═",COL_R) << "╣\n" << R;
}

// ── Public API ────────────────────────────────────────────────────────────────
void print_version() {
    // BUILD_DATE is injected by CMake at compile time
    std::cout << c(col::BCYAN, "⚡ Kinetic") << " version "
              << c(col::BOLD, "1.0.0")
              << c(col::DIM,  "  build:" KINETIC_BUILD_DATE)
              << "\n";
}

void print_help() {
    const char* B  = "\033[1m";     // bold  (borders)
    const char* C  = "\033[1;36m"; // bold cyan  (section headings)
    const char* Y  = "\033[1;33m"; // bold yellow (flags)
    const char* G  = "\033[32m";   // green (examples left)
    const char* DM = "\033[2m";    // dim   (hints / continuation)
    const char* NO = "\033[0m";    // plain (normal text)
    const char* R  = "\033[0m";    // reset

    if (!g_color_enabled) {
        B = C = Y = G = DM = NO = R = "";
    }

    // ╔ Top border ╗
    std::cout << B << "╔" << rep("═", BOX_W) << "╗\n" << R;

    // Title block
    row("          \xe2\x9a\xa1  K I N E T I C   B U I L D   S Y S T E M",  C,  B, R);
    row("               Android  \xe2\x80\xa2  C++  \xe2\x80\xa2  Java  \xe2\x80\xa2  Kotlin",       NO, B, R);
    row("                         Version 1.0",                           NO, B, R);

    // USAGE
    hr(B,R);
    row("  USAGE:  ./kinetic [command] [options]", NO, B, R);

    // ── COMMANDS ──────────────────────────────────────────────────────────────
    hr(B,R);  row("  COMMANDS", C, B, R);  hr2(B,R);
    row2("  --build",   Y,  "  Full build pipeline (default command)",      NO, B, R);
    row2("  --clean",   Y,  "  Remove build/ directory entirely",            NO, B, R);
    row2("  --rebuild", Y,  "  Clean then full build",                       NO, B, R);
    row2("  --install", Y,  "  Build + push APK via adb install",            NO, B, R);
    row2("  --run",     Y,  "  Build, install, then launch app via adb",     NO, B, R);
    row2("  --check",   Y,  "  Validate kinetic.cmake only, no build",       NO, B, R);
    row2("  --env",     Y,  "  Print discovered SDK/NDK/AAPT2 paths",        NO, B, R);
    row2("  --version", Y,  "  Print Kinetic version and exit",              NO, B, R);
    row2("  --help",    Y,  "  Show this help panel",                        NO, B, R);

    // ── BUILD OPTIONS ─────────────────────────────────────────────────────────
    hr2c(B,R);  row("  BUILD OPTIONS", C, B, R);  hr2(B,R);
    row2("  --release",   Y,  "  Build in release mode (optimized, signed)",  NO, B, R);
    row2("  --debug",     Y,  "  Build in debug mode (default)",               NO, B, R);
    row2("  --abi <abi>", Y,  "  Build for specific ABI only",                 NO, B, R);
    row2("",              DM, "  Values: arm64-v8a | armeabi-v7a | x86_64",    NO, B, R);
    row2("  --jobs <n>",  Y,  "  Parallel compile jobs (default: auto)",       NO, B, R);
    row2("  --no-dex",    Y,  "  Skip Dex conversion (C++ only apps)",         NO, B, R);
    row2("  --no-sign",   Y,  "  Skip APK signing step",                       NO, B, R);

    // ── OUTPUT & LOGGING ──────────────────────────────────────────────────────
    hr2c(B,R);  row("  OUTPUT & LOGGING OPTIONS", C, B, R);  hr2(B,R);
    row2("  --verbose",   Y,  "  Print every command and file operation",      NO, B, R);
    row2("  --quiet",     Y,  "  Suppress per-file output, show summary only", NO, B, R);
    row2("  --no-color",  Y,  "  Disable ANSI color output",                   NO, B, R);
    row2("  --log <f>",   Y,  "  Write full build log to file",                NO, B, R);
    row2("  --phase <n>", Y,  "  Run a single phase only (for debugging)",     NO, B, R);
    row2("",              DM, "  Values: env|cmake|res|ndk|java|kotlin|",       NO, B, R);
    row2("",              DM, "          dex|copy|pack|sign",                   NO, B, R);

    // ── ENVIRONMENT OVERRIDES ─────────────────────────────────────────────────
    hr2c(B,R);  row("  ENVIRONMENT OVERRIDES", C, B, R);  hr2(B,R);
    row2("  --sdk <p>",   Y, "  Override auto-discovered SDK path",            NO, B, R);
    row2("  --ndk <p>",   Y, "  Override auto-discovered NDK path",            NO, B, R);
    row2("  --aapt2 <p>", Y, "  Override system AAPT2 path (aarch64)",         NO, B, R);

    // ── EXAMPLES (wider left column, no ║ divider) ────────────────────────────
    hr2c(B,R);  row("  EXAMPLES", C, B, R);  hr(B,R);
    ex_row("  ./kinetic",                         G, "  Full debug build",         NO, B, R);
    ex_row("  ./kinetic --release",               G, "  Release build",            NO, B, R);
    ex_row("  ./kinetic --run",                   G, "  Build + install + launch", NO, B, R);
    ex_row("  ./kinetic --build --abi arm64-v8a", G, "  Single ABI build",         NO, B, R);
    ex_row("  ./kinetic --build --verbose",       G, "  Verbose build",            NO, B, R);
    ex_row("  ./kinetic --env",                   G, "  Check environment setup",  NO, B, R);

    // ── Footer ────────────────────────────────────────────────────────────────
    hr(B,R);
    row("  Place android-sdk/ and android-ndk/ in $HOME",        DM, B, R);
    row("  Place kinetic.cmake in your project root",             DM, B, R);
    row("  Install system AAPT2 via:  pkg install aapt2",         DM, B, R);

    // ╚ Bottom border ╝
    std::cout << B << "╚" << rep("═", BOX_W) << "╝\n" << R << "\n";
}

} // namespace kinetic
