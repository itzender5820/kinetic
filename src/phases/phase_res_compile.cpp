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
    const char* prefix_env = std::getenv("PREFIX");
    if (prefix_env) {
        std::string prefix_aapt2 = std::string(prefix_env) + "/bin/aapt2";
        if (access(prefix_aapt2.c_str(), X_OK) == 0) return prefix_aapt2;
    }
    return which("aapt2");
}

static bool needs_legacy(const fs::path& file) {
    const auto ext = file.extension().string();
    return ext == ".png" || ext == ".webp" || ext == ".jpg" || ext == ".gif";
}

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

    err_out = r.err.empty() ? r.out : r.err;
    if (err_out.empty()) {
        err_out = "aapt2 exited with code " + std::to_string(r.exit_code)
                + " and produced no output.\n"
                + "Try running manually:\n  "
                + aapt2.string() + " compile -o /tmp/ " + file.string();
    }
    return false;
}

void PhaseResCompile::execute(PhaseContext& ctx) {
    const fs::path res_dir = ctx.project_root / "src" / "main" / "res";

    if (!fs::exists(res_dir)) {
        fatal("RES_COMPILE", err::RES_001,
              "res/ directory not found", res_dir.string(),
              "Create src/main/res/ with at least values/strings.xml");
    }

    std::error_code ec;
    fs::create_directories(ctx.dirs.flat_dir, ec);

    std::vector<fs::path> res_files;
    for (const auto& entry : fs::recursive_directory_iterator(res_dir)) {
        if (entry.is_regular_file()) res_files.push_back(entry.path());
    }
    if (res_files.empty()) {
        phase_warn("RES_COMPILE", "No resource files found in res/");
        ctx.timer.start("RES_COMPILE");
        ctx.timer.stop(true, "0 .flat files");
        return;
    }

    ctx.timer.start("RES_COMPILE");
    phase_log("RES_COMPILE", "Compiling "
              + std::to_string(res_files.size()) + " resources ...");

    fs::path aapt2                  = ctx.env.sdk_aapt2;
    bool     native_fallback_done   = false;
    int      compiled               = 0;

    for (const auto& res_file : res_files) {
        std::string err_out;
        bool ok = compile_one(aapt2, res_file, ctx.dirs.flat_dir,
                              ctx.project_root, err_out);

        if (!ok && is_x86_elf_error(err_out)
                && host_is_aarch64() && !native_fallback_done) {
            native_fallback_done = true;
            const std::string native = find_native_aapt2();
            if (!native.empty() && native != aapt2.string()) {
                phase_warn("RES_COMPILE",
                    "Switching to native aarch64 aapt2 → " + native);
                aapt2 = fs::path(native);
                err_out.clear();
                ok = compile_one(aapt2, res_file, ctx.dirs.flat_dir,
                                 ctx.project_root, err_out);
            }
        }

        if (!ok) {
            ctx.timer.stop(false, "failed");
            fatal("RES_COMPILE", err::RES_002,
                  "aapt2 compile failed: " + res_file.filename().string(),
                  res_file.string(), err_out);
        }

        phase_log("RES_COMPILE",
                  fs::relative(res_file, res_dir).string() + "  → .flat");
        ++compiled;
    }

    phase_log("RES_COMPILE", "Done  (" + std::to_string(compiled) + " files compiled)");
    ctx.timer.stop(true, std::to_string(compiled) + " .flat files");
}

} // namespace kinetic
