// ─────────────────────────────────────────────────────────────────────────────
//  dag/executor.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "executor.hpp"
#include "../kinetic.hpp"

#include <iostream>
#include <queue>
#include <stdexcept>

namespace kinetic {

namespace {

// Encapsulate a phase body in a std::packaged_task so it produces a
// std::future<void>.  The const Dag must outlive the task.
struct PhaseTask {
    const std::string   phase_name;
    std::function<void()> body;
    std::shared_future<void> fut;

    PhaseTask(const std::string& n, std::function<void()> b)
        : phase_name(n), body(std::move(b)) {}

    void run() {
        body();
    }
};

} // namespace

Executor::Executor(Dag& dag, int jobs_hint)
    : dag_(dag), jobs_hint_(jobs_hint) {}

size_t Executor::compute_jobs() const
{
    if (jobs_hint_ > 0) return (size_t)jobs_hint_;
    const auto hw = std::thread::hardware_concurrency();
    if (hw == 0) return 1;
    return hw;
}

bool Executor::depends_on(const std::string& name,
                          const std::string& target) const
{
    // BFS: does `target` require `name`?  i.e. follow target.requires() to
    // see if we eventually arrive at `name`.
    if (target == name) return true;
    std::unordered_set<std::string> seen;
    std::queue<std::string> q;
    q.push(target);
    seen.insert(target);
    while (!q.empty()) {
        auto cur = q.front(); q.pop();
        const Phase* p = dag_.find(cur);
        if (!p) continue;
        for (const auto& dep : p->requires()) {
            if (dep == name) return true;
            if (seen.insert(dep).second) q.push(dep);
        }
    }
    return false;
}

std::unordered_set<std::string> Executor::closure(const std::string& single_phase) const
{
    std::unordered_set<std::string> out;
    if (single_phase.empty()) return out;
    // Walk requires():
    std::queue<std::string> q;
    q.push(single_phase);
    out.insert(single_phase);
    while (!q.empty()) {
        auto cur = q.front(); q.pop();
        const Phase* p = dag_.find(cur);
        if (!p) continue;
        for (const auto& dep : p->requires()) {
            if (out.insert(dep).second) q.push(dep);
        }
    }
    return out;
}

int Executor::run(PhaseContext& ctx)
{
    // Validate+determine execution order.
    std::string err;
    if (!dag_.validate(err)) {
        std::cerr << "kinetic DAG invalid: " << err << "\n";
        return 1;
    }
    const auto order = dag_.topo_order();
    if (order.empty()) return 0;

    // Compute the subset to actually run.
    std::unordered_set<std::string> active;
    if (!ctx.single_phase.empty()) {
        active = closure(ctx.single_phase);
        if (active.empty()) {
            std::cerr << "phase '" << ctx.single_phase << "' not registered\n";
            return 1;
        }
    } else {
        for (const auto& n : order) active.insert(n);
    }

    // Load the incremental cache.
    hasher_.load(ctx.dirs.stamps_file);

    // Determine the set of cacheable phases eligible for skipping.
    // A phase is skippable iff:
    //   1. it is cacheable()
    //   2. its stamp matches AND its provided files exist on disk
    //   3. the user did not explicitly request it via --phase NAME
    std::unordered_set<std::string> skip_set;
    for (const auto& n : order) {
        if (!active.count(n)) continue;
        Phase* p = dag_.find(n);
        if (!p || !p->cacheable()) continue;
        if (!ctx.single_phase.empty() && n == ctx.single_phase) {
            // Force re-run of the targeted phase.
            continue;
        }
        if (hasher_.is_clean(*p, ctx)) skip_set.insert(n);
    }

    // Track completion via futures keyed by phase name.  Each future
    // resolves when its phase body completes successfully OR throws on
    // failure.  Futures are stored in `futures_` keyed by name; downstream
    // phases wait on every required predecessor's future.
    futures_.clear();
    results_.clear();
    results_.reserve(order.size());
    cached_set_.clear();
    cached_set_ = skip_set;

    // Using a shared thread pool sized to `compute_jobs()`; cacheable-skip
    // phases use negligible time.
    const size_t max_workers = std::min(compute_jobs(), order.size());
    if (max_workers < 1) {
        // Single-threaded fallback.
    }

    // We use an explicit worker pool so that the executor's lifetime owns
    // the threads.  For phases that are non-parallelizable we serialize
    // them on a per-name "main thread token": the worker function waits
    // on a global serial mutex while the phase body executes.
    std::mutex serial_mtx;

    // Bookkeeping for results: keyed by phase name.
    std::unordered_map<std::string, PhaseExecResult> result_map;
    for (const auto& n : order) result_map[n].phase_name = n;

    // Submit each active phase.  Each phase's task does the following:
    //   1. wait on every requires() predecessor's future
    //   2. if any predecessor failed with stored exception: cancel self
    //   3. if in skip_set: record cached result and continue
    //   4. else: invoke phase.execute(ctx) under ctx.timer, capture exc
    //   5. record PhaseExecResult + propagate failure to successors
    for (const auto& n : order) {
        if (!active.count(n)) continue;
        Phase* p = dag_.find(n);
        if (!p) continue;

        // Collect the predecessor futures first.
        std::vector<std::shared_future<void>> pred_futs;
        for (const auto& dep : p->requires()) {
            auto it = futures_.find(dep);
            if (it != futures_.end()) pred_futs.push_back(it->second);
        }

        // Skip cached phase: record result and an empty completed future.
        if (skip_set.count(n) > 0) {
            PhaseExecResult r;
            r.phase_name   = n;
            r.success      = true;
            r.cached_skip  = true;
            r.output_desc  = "(cached)";
            result_map[n]  = r;

            std::promise<void> pr; pr.set_value();
            futures_[n] = pr.get_future().share();
            continue;
        }

        std::shared_future<void> fut;
        bool do_parallel = p->parallelizable() && max_workers > 1;

        if (do_parallel) {
            // Stand-alone std::async so that futures carry the exception.
            std::promise<void> pr;
            fut = pr.get_future().share();

            // Copy predecessor futures so the task's lambda captures local snapshots.
            std::vector<std::shared_future<void>> snapshot_maps = pred_futs;
            std::string phase_name = n;
            PhaseExecResult* result_slot = &result_map[n];
            Phase* phase_ptr = p;
            // Capture the "first error" by reference through the mutex.
            std::mutex* em = &mtx_;

            std::async(std::launch::async,
                [&, phase_ptr, phase_name, snapshot_maps, pr = std::move(pr),
                 result_slot, em]() mutable {
                    // Wait for every predecessor.
                    bool pred_failed = false;
                    for (auto& f : snapshot_maps) {
                        try { f.get(); }
                        catch (...) { pred_failed = true; }
                    }
                    if (pred_failed) {
                        result_slot->success = false;
                        result_slot->output_desc = "skipped (upstream failed)";
                        // Mark future as completed (success in the sense of
                        // "the task chose not to run"); downstream will
                        // observe its requires() fullfiled through the
                        // dependency-waiter, then also fail its own pred_failed.
                        pr.set_value();
                        return;
                    }

                    // Check first_err_.
                    {
                        std::lock_guard<std::mutex> lk(*em);
                        if (first_err_) {
                            result_slot->success = false;
                            result_slot->output_desc = "skipped (upstream failed)";
                            pr.set_value();
                            return;
                        }
                    }

                    try {
                        auto t0 = std::chrono::steady_clock::now();
                        ctx.timer.start(phase_name);
                        if (phase_ptr->parallelizable() == false) {
                            std::lock_guard<std::mutex> lk(serial_mtx);
                            phase_ptr->execute(ctx);
                        } else {
                            phase_ptr->execute(ctx);
                        }
                        auto t1 = std::chrono::steady_clock::now();
                        result_slot->duration_ns =
                            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                        result_slot->success      = true;
                        const auto* rec = ctx.timer.current();
                        result_slot->output_desc  = (rec && !rec->output_desc.empty()) ? rec->output_desc : "ok";
                        ctx.timer.stop(true, result_slot->output_desc);

                        if (phase_ptr->cacheable()) {
                            // Update stamp after successful run.
                            hasher_.mark_done(*phase_ptr, ctx);
                        }
                        pr.set_value();
                    } catch (...) {
                        auto t1 = std::chrono::steady_clock::now();
                        result_slot->duration_ns =
                            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - std::chrono::steady_clock::now()).count();
                        result_slot->success      = false;
                        result_slot->output_desc  = "failed";
                        ctx.timer.stop(false, "failed");
                        auto ep = std::current_exception();
                        {
                            std::lock_guard<std::mutex> lk(*em);
                            if (!first_err_) first_err_ = ep;
                        }
                        try { std::rethrow_exception(ep); }
                        catch (const std::exception& e) {
                            result_slot->error_text = e.what();
                        }
                        try { pr.set_exception(ep); }
                        catch (...) { pr.set_value(); } // future already satisfied
                    }
                });
            // std::async returns a future; we don't need it ourselves —
            // we hold it through fut.  But std::async's returned future
            // has different semantics (it blocks in dtor).  Wrap.
            // The shared future we held is the one we just set.
            continue;
        }

        // Serial path (max_workers == 1 or phase not parallelizable): run
        // synchronously on the calling thread.
        {
            bool pred_failed = false;
            for (auto& f : pred_futs) {
                try { f.get(); }
                catch (...) { pred_failed = true; }
            }
            if (pred_failed) {
                result_map[n].success = false;
                result_map[n].output_desc = "skipped (upstream failed)";
                std::promise<void> pr; pr.set_value();
                futures_[n] = pr.get_future().share();
                continue;
            }
            {
                std::lock_guard<std::mutex> lk(mtx_);
                if (first_err_) {
                    result_map[n].success = false;
                    result_map[n].output_desc = "skipped (upstream failed)";
                    std::promise<void> pr; pr.set_value();
                    futures_[n] = pr.get_future().share();
                    continue;
                }
            }

            try {
                ctx.timer.start(n);
                if (!p->parallelizable()) {
                    std::lock_guard<std::mutex> lk(serial_mtx);
                    p->execute(ctx);
                } else {
                    p->execute(ctx);
                }
                ctx.timer.stop(true, "ok");
                result_map[n].success = true;
                const auto* rec = ctx.timer.current();
                result_map[n].output_desc = (rec && !rec->output_desc.empty()) ? rec->output_desc : "ok";
                result_map[n].duration_ns = ctx.timer.current() ? ctx.timer.current()->duration_ns : 0;
                if (p->cacheable()) hasher_.mark_done(*p, ctx);
                std::promise<void> pr; pr.set_value();
                futures_[n] = pr.get_future().share();
            } catch (const std::exception& e) {
                ctx.timer.stop(false, "failed");
                result_map[n].success = false;
                result_map[n].output_desc = "failed";
                result_map[n].error_text = e.what();
                std::lock_guard<std::mutex> lk(mtx_);
                if (!first_err_) first_err_ = std::current_exception();
                // Still publish a completed future so successors skip cleanly.
                std::promise<void> pr; pr.set_value();
                futures_[n] = pr.get_future().share();
            } catch (...) {
                ctx.timer.stop(false, "failed");
                result_map[n].success = false;
                result_map[n].output_desc = "failed";
                std::lock_guard<std::mutex> lk(mtx_);
                if (!first_err_) first_err_ = std::current_exception();
                std::promise<void> pr; pr.set_value();
                futures_[n] = pr.get_future().share();
            }
        }
    }

    // Wait for every submitted future (ensures async tasks complete).
    for (const auto& n : order) {
        if (!active.count(n)) {
            // Skip phases that the user did not include in this single
            // run.  Their futures were never set, so we silently ignore.
            continue;
        }
        auto it = futures_.find(n);
        if (it != futures_.end()) {
            try { it->second.get(); }
            catch (...) { /* already captured */ }
        }
    }

    // Re-load results into the vector in topo order.
    for (const auto& n : order) {
        if (!active.count(n)) continue;
        if (result_map.count(n) == 0) {
            PhaseExecResult r;
            r.phase_name = n;
            r.success    = false;
            r.output_desc = "not run";
            results_.push_back(r);
        } else {
            results_.push_back(result_map[n]);
        }
    }

    // Persist stamps (skips need to be preserved for the next run too).
    hasher_.save();

    // Return 1 on any failure.
    for (const auto& r : results_) {
        if (!r.success) return 1;
    }
    return 0;
}

} // namespace kinetic
