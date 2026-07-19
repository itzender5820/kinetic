// ─────────────────────────────────────────────────────────────────────────────
//  dag/executor.hpp  —  Parallel phase runner driven by the DAG + FSHasher.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "dag.hpp"
#include "fshasher.hpp"
#include "phase.hpp"

#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kinetic {

// Result record for a single phase after the graph completes.
struct PhaseExecResult {
    std::string phase_name;
    bool        success;
    bool        cached_skip;
    long long   duration_ns;
    std::string output_desc;   // short summary, e.g. "3 .so files"
    std::string error_text;    // empty on success
};

class Executor {
public:
    // `dag` must outlive the runner.  `jobs_hint` <=0 means auto (= N from
    // std::thread::hardware_concurrency()).
    Executor(Dag& dag, int jobs_hint = 0);

    // Run the graph in topological order.  Phases that can be satisfied
    // concurrently and are parallelizable run in parallel; non-parallel
    // phases run serially on the main thread.
    //
    // If `single_phase` is non-empty, only that phase (and its required
    // predecessors, transitively) runs.
    //
    // On any phase failure, downstream phases are skipped; ancestor
    // phases of the failed phase still complete.
    int run(PhaseContext& ctx);

    // Read-only access to per-phase results.
    const std::vector<PhaseExecResult>& results() const { return results_; }

    // Was a phase skipped-as-cached in the most recent run?
    bool was_cached(const std::string& phase_name) const {
        return cached_set_.count(phase_name) > 0;
    }

private:
    Dag&        dag_;
    FSHasher   hasher_;
    int        jobs_hint_;

    std::vector<PhaseExecResult>                results_;
    std::unordered_set<std::string>            cached_set_;
    std::unordered_map<std::string, std::shared_future<void>> futures_;

    // Shares the exception_ptr from failed phases so successors fail
    // (skip) cleanly.
    std::mutex                  mtx_;
    std::exception_ptr          first_err_ = nullptr;

    size_t compute_jobs() const;

    // Return true if `name` is reachable from `target` in the requires
    // graph (i.e. target depends transitively on name).
    bool  depends_on(const std::string& name, const std::string& target) const;

    // Collect the transitive closure of `single_phase`'s required phases.
    std::unordered_set<std::string> closure(const std::string& single_phase) const;
};

} // namespace kinetic
