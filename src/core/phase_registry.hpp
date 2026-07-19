// ─────────────────────────────────────────────────────────────────────────────
//  core/phase_registry.hpp  —  Factory that registers all Phase subclasses
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "../dag/dag.hpp"

namespace kinetic {

// Register every Phase subclass into the DAG in the order they execute.
void register_all_phases(Dag& dag);

} // namespace kinetic
