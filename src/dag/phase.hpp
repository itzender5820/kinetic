// ─────────────────────────────────────────────────────────────────────────────
//  dag/phase.hpp  —  Abstract Phase base class + PhaseContext
//
//  A Phase is one unit of work in the build graph.  Each existing free
//  function in src/phases/*.cpp becomes a subclass that:
//     • name()         — stable identifier used by --phase NAME and by edges
//     • requires()     — names of phases that must finish before this one
//     • provides()     — list of file artifacts produced by execute()
//     • parallelizable()— false for phases that mutate shared state
//     • cacheable()    — true when FSHasher may skip the phase on a cache hit
//     • execute(ctx)   — does the actual work, throws on hard failure
//
//  PhaseContext holds everything a phase needs: env, config, project root,
//  all derived build directories, the timer, and an error collector.  All
//  phases share one context, allocated once by the engine.
//
//  The contract:
//   - name() returns the same string for the lifetime of the instance.
//   - requires()/provides() are stable for a given configuration.
//   - execute() is called at most once per graph run; it must not begin any
//     side-effects until execute() is entered (the executor may swap them
//     between threads before their turn).
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../kinetic.h"
#include "../kinetic.hpp"
#include "../config/config_model.hpp"
#include "../env/env_scanner.hpp"
#include "../core/phase_timer.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace kinetic {

namespace fs = std::filesystem;

// Build-directory layout shared across all phases.  Filled in by the engine
// after config parsing and before any non-trivial phase runs.
struct BuildDirs {
    fs::path build_dir;
    fs::path flat_dir;
    fs::path aprs_dir;
    fs::path obj_dir;
    fs::path lib_dir;
    fs::path classes_dir;
    fs::path dex_dir;
    fs::path staging_dir;
    fs::path output_dir;
    fs::path cache_dir;       // build/.kinetic-cache/
    fs::path stamps_file;     // cache_dir / stamps.tsv
};

// PhaseContext is constructed by the engine after ENV_SCAN + CMAKE_PARSE
// and shared (by const-ref) with every subsequent phase.
struct PhaseContext {
    // Discovered environment — copy owned by the context.
    KineticEnv env;

    // Parsed configuration — copy owned by the context.
    KineticConfig cfg;

    // Absolute project root (the directory that contains kinetic.cmake).
    fs::path project_root;

    // All derived build directory paths — see BuildDirs above.
    BuildDirs dirs;

    // Shared phase timer.  Phases call start()/stop() around their work.
    PhaseTimer timer;

    // CLI options (read-only).  Stored as the C++ struct from cli_parser.hpp.
    // Forward-declared in cli_parser.hpp as a complete type.
    // To avoid a circular dependency (cli_parser does not include dag), we
    // store the few fields the phases need as plain members here, set by
    // the engine when constructing the context.
    bool verbose       = false;
    bool quiet         = false;
    bool no_color      = false;
    bool no_dex         = false;
    bool no_sign       = false;
    bool release_mode  = false;
    int  jobs          = 0;       // 0 = auto
    std::string abi_filter;      // empty = use cfg.abi_filters
    std::string single_phase;    // --phase NAME (empty = run all)

    // Environment override paths from CLI (--sdk, --ndk, --aapt2).
    std::string sdk_override;
    std::string ndk_override;
    std::string aapt2_override;

    // kinetic.cmake file name (default: "kinetic.cmake").
    std::string cmake_file = "kinetic.cmake";

    // Java files that were compiled (set by PhaseJavaCompile, read by PhaseKotlinCompile).
    std::vector<fs::path> compile_classes_;
};

class Phase {
public:
    virtual ~Phase() = default;

    // Stable, unique name used to identify this phase in the DAG and the
    // --phase flag.
    virtual const char* name() const = 0;

    // Names of phases whose outputs this phase requires.  May be empty.
    // All returned names MUST exist as registered phases in the same DAG.
    virtual std::vector<std::string> requires() const = 0;

    // Filesystem paths produced by this phase.  Used by the hash-based
    // incremental cache.  Phases with no on-disk outputs should return an
    // empty vector and set cacheable() to false.
    virtual std::vector<fs::path> provides(const PhaseContext&) const = 0;

    // Phases that touch shared mutable state (e.g. config, env) must
    // return false.  Default true permits parallel execution.
    virtual bool parallelizable() const { return true; }

    // Whether the runner may skip this phase when its inputs/stamps match
    // the cache.  Phases that always need to run (manifest validation, env
    // scan) should return false.
    virtual bool cacheable() const { return false; }

    // Run the phase.  Throws std::runtime_error on hard failure.
    // The signature here differs from the previous free-function style:
    // phases now own a reference to the shared context, and may read or
    // mutate it as needed (e.g. CMAKE_PARSE stores cfg into ctx).
    virtual void execute(PhaseContext& ctx) = 0;
};

} // namespace kinetic
