// ─────────────────────────────────────────────────────────────────────────────
//  dag/fshasher.hpp  —  Incremental build cache.
//
//  For every phase whose cacheable() returns true, the FSHasher builds a
//  deterministic string fingerprint derived from the phase's own inputs
//  (its required predecessors' outputs) and the phase's static
//  configuration (relevant KineticConfig fields).  When the previously
//  recorded fingerprint matches the freshly-computed one and all the
//  phase's provided artifacts exist on disk, the executor may skip the
//  phase.
//
//  Storage format:
//   build/.kinetic-cache/stamps.tsv
//   one phase per line: <phase_name>\t<stamp_hex>
//
//  The file is regenerated each run with the new state of every cacheable
//  phase — cacheable phases that ran are updated with the new stamp;
//  non-cacheable phases are absent.  This is a "first run everything, then
//  drift-only rerun" scheme — simpler and far less brittle than incremental
//  clean-up.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "phase.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace kinetic {

class FSHasher {
public:
    FSHasher() = default;

    // Read the on-disk stamp table from attrs.stamps_file.  Returns the
    // number of entries loaded (or 0 if the file does not exist yet).
    size_t load(const fs::path& stamps_file);

    // Build a stamp for `phase` using its `requires` predecessors' actual
    // outputs on disk.  Phase `phase` is treated as if its inputs include
    // every file produced by any of its requires() siblings.  The stamp
    // incorporates the SHA-256 of every such input file plus a stable
    // set of KineticConfig fields that affect compilation linkage.
    //
    // Returns the hex digest (64 chars) or an empty string if no inputs exist.
    std::string compute_stamp(const Phase& phase, const PhaseContext& ctx) const;

    // True when this phase's current stamp matches the recorded one AND
    // every `provides()` file currently exists on disk.
    bool is_clean(const Phase& phase, const PhaseContext& ctx) const;

    // Record the new stamp for `phase` in-memory.  Caller is expected to
    // call save() afterwards to persist.
    void mark_done(const Phase& phase, const PhaseContext& ctx);

    // Drop a phase from the in-memory table.  Used when a cached phase
    // detects an upstream change that should propagate downstream.
    void invalidate(const std::string& phase_name);

    // Persist to the on-disk file.  File is rewritten atomically (temp +
    // rename).  Returns false on write failure.
    bool save() const;

    // Stamp for a phase (looked up from the in-memory table after a
    // load()).  Empty string if not in the table yet.
    std::string stamp(const std::string& phase_name) const;

    // Number of phases with recorded stamps.
    size_t size() const { return last_.size(); }

    const fs::path& stamps_file() const { return stamps_file_; }

private:
    fs::path stamps_file_;
    std::unordered_map<std::string, std::string> last_;  // phase → stamp hex
};

} // namespace kinetic
