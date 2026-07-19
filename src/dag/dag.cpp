// ─────────────────────────────────────────────────────────────────────────────
//  dag/dag.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "dag.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace kinetic {

Dag::Dag() = default;

bool Dag::add(std::unique_ptr<Phase> phase)
{
    if (!phase) return false;
    const std::string n = phase->name();
    if (by_name_.count(n) > 0) return false;

    Phase* raw = phase.get();
    // Compute edges eagerly so topo_order can be const-safe without
    // re-evaluating requires().
    edges_[n] = phase->requires();

    phases_.push_back(std::move(phase));
    by_name_[n] = raw;
    return true;
}

const Phase* Dag::find(const std::string& n) const
{
    auto it = by_name_.find(n);
    if (it == by_name_.end()) return nullptr;
    return it->second;
}

Phase* Dag::find(const std::string& n)
{
    auto it = by_name_.find(n);
    if (it == by_name_.end()) return nullptr;
    return it->second;
}

bool Dag::has_cycle_dfs(const std::string& node,
                         std::unordered_set<std::string>& visiting,
                         std::unordered_set<std::string>& visited) const
{
    if (visiting.count(node) > 0) return true; // back edge → cycle
    if (visited.count(node) > 0) return false;
    visiting.insert(node);

    auto it = edges_.find(node);
    if (it != edges_.end()) {
        for (const auto& dep : it->second) {
            if (has_cycle_dfs(dep, visiting, visited)) return true;
        }
    }
    visiting.erase(node);
    visited.insert(node);
    return false;
}

bool Dag::validate(std::string& err) const
{
    // 1. All edges reference names that exist.
    for (const auto& kv : edges_) {
        for (const auto& dep : kv.second) {
            if (by_name_.count(dep) == 0) {
                err = "phase '" + kv.first + "' requires unknown phase '" + dep + "'";
                return false;
            }
        }
    }
    // 2. Acyclic check.
    std::unordered_set<std::string> visiting, visited;
    for (const auto& p : phases_) {
        if (has_cycle_dfs(p->name(), visiting, visited)) {
            err = std::string("cycle detected in DAG involving phase '") + p->name() + "'";
            return false;
        }
    }
    return true;
}

std::vector<std::string> Dag::topo_order() const
{
    std::vector<std::string> result;
    result.reserve(phases_.size());
    if (phases_.empty()) return result;

    // Compute indegree for every node.
    std::unordered_map<std::string, size_t> indeg;
    for (const auto& p : phases_) indeg[p->name()] = 0;
    for (const auto& kv : edges_) {
        // kv.second is the list of names kv.first depends on.  Each such
        // name must run BEFORE kv.first; so the "edge direction" for
        // topological purposes goes dep → kv.first.
        indeg[kv.first] += kv.second.size();
    }

    // Use a queue but preserve registration order on ties.  We do this by
    // choosing the registration-order index when phases share indegree 0.
    // Simplest deterministic approach: iterate indices in ascending order
    // when looking for nodes to emit.
    // (std::priority_queue would re-order alphabetically, which would
    // change the legacy phase sequence — we want it preserved.)

    std::vector<bool> emitted(phases_.size(), false);

    // Repeat N times; each pass scans in registration order and emits the
    // first unfinished node with indegree 0.  That keeps the legacy order
    // intact for parallelizable sub-chains (no shuffle).
    for (size_t emitted_count = 0; emitted_count < phases_.size(); ) {
        bool progressed = false;
        for (size_t i = 0; i < phases_.size(); ++i) {
            if (emitted[i]) continue;
            const std::string& n = phases_[i]->name();
            if (indeg[n] == 0) {
                result.push_back(n);
                emitted[i] = true;
                ++emitted_count;
                progressed = true;
                // Decrement indeg of phases that depend on n.
                for (size_t j = 0; j < phases_.size(); ++j) {
                    auto it = edges_.find(phases_[j]->name());
                    if (it == edges_.end()) continue;
                    for (const auto& dep : it->second) {
                        if (dep == n) {
                            --indeg[phases_[j]->name()];
                        }
                    }
                }
            }
        }
        if (!progressed) {
            // Cycle remaining — return what we have.  Caller should have
            // validate() first to avoid this.
            break;
        }
    }

    return result;
}

} // namespace kinetic
