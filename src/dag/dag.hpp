// ─────────────────────────────────────────────────────────────────────────────
//  dag/dag.hpp  —  Phase graph: registration, cycle detection, topo order
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "phase.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kinetic {

// A Dag owns its phases.  It exposes:
//   • add()          — register a phase
//   • validate()     — check that all requires() targets exist and the
//                       graph has no cycles (DFS-based)
//   • topo_order()   — return phase names in execution order
//   • find()         — lookup by name
class Dag {
public:
    Dag();

    // Take ownership of `phase`.  Returns false if a phase with the same
    // name is already registered.
    bool add(std::unique_ptr<Phase> phase);

    // Validate edges + acyclicity.  On success returns true.  On failure
    // fills `err` with a human-readable description and returns false.
    bool validate(std::string& err) const;

    // Topologically sorted phase names.  Result is deterministic for a
    // given input order: ties in the Kahn's-algorithm queue are broken by
    // registration order, so the legacy phase sequence is preserved tick
    // for tick when no parallelism is requested.  Returns an empty vector
    // if validate() would fail (call validate() first to get the error).
    std::vector<std::string> topo_order() const;

    // Lookup.  Returns nullptr if not found.
    const Phase* find(const std::string& name) const;
    Phase*       find(const std::string& name);

    // All registered phases, in registration order.
    const std::vector<std::unique_ptr<Phase>>& phases() const { return phases_; }

    // Predecessor lookup derived from requires().  Filled by validate().
    // Each name maps to the set of names listed by that phase's requires().
    const std::unordered_map<std::string, std::vector<std::string>>& edges() const {
        return edges_;
    }

private:
    std::vector<std::unique_ptr<Phase>>                 phases_;
    std::unordered_map<std::string, Phase*>             by_name_;
    std::unordered_map<std::string, std::vector<std::string>> edges_;

    // Helper for cycle detection: returns true when a DFS starting at
    // `start` encounters `start` again (back edge).
    bool has_cycle_dfs(const std::string& start,
                       std::unordered_set<std::string>& visiting,
                       std::unordered_set<std::string>& visited) const;
};

} // namespace kinetic
