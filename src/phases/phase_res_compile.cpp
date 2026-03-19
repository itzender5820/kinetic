// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_res_compile.cpp  —  Phase 03: RES_COMPILE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_res_compile.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <unistd.h>
#include <sys/utsname.h>

namespace kinetic {

static bool is_x86_elf_error(const std::string& s) {
    return s.find("Syntax error: word unexpected") != std::string::npos
        || s.find("Exec format error")             != std::string::npos
        || s.find("cannot execute binary file")    != std::string::npos;
}

static bool host_is_aarch64() {
    struct utsname u{}; uname(&u);
    std::string m(u.machine);
    return m == "aarch64" || m == "arm64";
}

static std::string find_native_aapt2() {
    char buf[4096]{};
    if (readlink("/proc/self/exe", buf, sizeof(buf) - 1) > 0) {
        fs::path self_dir = fs::path(std::string(buf)).parent_path();
        for (auto&& rel : {"assets/aapt2-aarch64", "../assets/aapt2-aarch64"}) {
            fs::path p = self_dir / rel;
            if (fs::exists(p)) return p.string();
        }
    }
    // $PREFIX/bin/aapt2 (Termux and any prefix-based install — never hardcode the path)
    const char* prefix_env = std::getenv("PREFIX");
    if (prefix_env) {
        std::string prefix_aapt2 = std::string(prefix_env) + "/bin/aapt2";
        if (access(prefix_aapt2.c_str(), X_OK) == 0) return prefix_aapt2;
    }
    return which("aapt2");
}

// --legacy is needed for PNG/nine-patch files only.
// XML vector drawables must NOT use --legacy — older aarch64 aapt2 builds
// crash silently on it (no output, non-zero exit).
static bool needs_legacy(const fs::path& file) {
    const auto ext = file.extension().string();
    return ext == ".png" || ext == ".webp" || ext == ".jpg" || ext == ".gif";
}

// Compile one resource file via exec_proc (pure pipe capture, no temp files).
static bool compile_one(const fs::path& aapt2,
                         const fs::path& file,
                         const fs::path& flat_dir,
                         const fs::path& project_root,
                         std::string&    err_out) {
    std::vector<std::string> cmd = {aapt2.string(), "compile"};
    if (needs_legacy(file)) cmd.push_back("--legacy");
    cmd.push_back("-o"); cmd.push_back(flat_dir.string());
    cmd.push_back(file.string());

    auto r = exec_proc(cmd, project_root);
    if (r.exit_code == 0) return true;

    // Prefer stderr; fall back to stdout; generate a message if both empty
    err_out = r.err.empty() ? r.out : r.err;
    if (err_out.empty()) {
        err_out = "aapt2 exited with code " + std::to_string(r.exit_code)
                + " and produced no output.\n"
                + "Try running manually:\n  "
                + aapt2.string() + " compile -o /tmp/ " + file.string();
// Note: on Termux use $TMPDIR instead of /tmp;
    }
    return false;
}

// ── Public entry point ────────────────────────────────────────────────────────
int phase_res_compile(const KineticConfig& cfg,
                      const KineticEnv& env,
                      const fs::path& project_root,
                      const fs::path& flat_dir) {
    (void)cfg;
    const fs::path res_dir = project_root / "src" / "main" / "res";

    if (!fs::exists(res_dir)) {
        fatal("RES_COMPILE", err::RES_001,
              "res/ directory not found", res_dir.string(),
              "Create src/main/res/ with at least values/strings.xml");
    }

    std::error_code ec;
    fs::create_directories(flat_dir, ec);

    std::vector<fs::path> res_files;
    for (const auto& entry : fs::recursive_directory_iterator(res_dir)) {
        if (entry.is_regular_file()) res_files.push_back(entry.path());
    }
    if (res_files.empty()) {
        phase_warn("RES_COMPILE", "No resource files found in res/");
        return 0;
    }

    phase_log("RES_COMPILE", "Compiling "
              + std::to_string(res_files.size()) + " resources ...");

    fs::path aapt2                  = env.sdk_aapt2;
    bool     native_fallback_done   = false;
    int      compiled               = 0;

    for (const auto& res_file : res_files) {
        std::string err_out;
        bool ok = compile_one(aapt2, res_file, flat_dir, project_root, err_out);

        // Auto-correct: wrong-arch binary detected
        if (!ok && is_x86_elf_error(err_out)
                && host_is_aarch64() && !native_fallback_done) {
            native_fallback_done = true;
            const std::string native = find_native_aapt2();
            if (!native.empty() && native != aapt2.string()) {
                phase_warn("RES_COMPILE",
                    "Switching to native aarch64 aapt2 → " + native);
                aapt2 = fs::path(native);
                err_out.clear();
                ok = compile_one(aapt2, res_file, flat_dir, project_root, err_out);
            }
        }

        if (!ok) {
            fatal("RES_COMPILE", err::RES_002,
                  "aapt2 compile failed: " + res_file.filename().string(),
                  res_file.string(), err_out);
        }

        phase_log("RES_COMPILE",
                  fs::relative(res_file, res_dir).string() + "  → .flat");
        ++compiled;
    }

    phase_log("RES_COMPILE", "Done  (" + std::to_string(compiled) + " files compiled)");
    return compiled;
}

} // namespace kinetic
