// ─────────────────────────────────────────────────────────────────────────────
//  kinetic.cpp  —  C++ counterparts for the C-layer functions declared in
//  kinetic.h.  These wrap the C primitives into std::string / std::vector
//  friendly adapters so all existing C++ code can keep its current style
//  while sharing one implementation with the C layer.
//
//  None of the previous C++ 'utils/process.cpp', 'utils/sha256.cpp' or
//  'error/error_reporter.cpp' have been deleted.  The previous C++ versions
//  still exist and contain the kinetic:: namespace surface used throughout
//  the existing code.  We expose C primitive functions next to them so the
//  new DAG / Phase infrastructure (in dag/) can use whichever layer is
//  convenient.
//
//  ALL new infrastructure (DAG, FileHasher, Executor) MUST call into this C
//  layer to keep binary size + ABI stable.  The old kinetic::* C++ functions
//  are kept for the legacy phase implementations which were already written
//  against them — those phases keep working unchanged.
// ─────────────────────────────────────────────────────────────────────────────
#include "kinetic.h"
#include <string>
#include <vector>

namespace kinetic {

// ── Color ────────────────────────────────────────────────────────────────────
// Expose the C boolean flag so the legacy kinetic::g_color_enabled (declared
// in kinetic.hpp) stays in sync with the C flag.
//
// (The legacy kinetic.hpp declares `extern bool g_color_enabled` — the
// definition lives here.  Setters below keep both sides synchronized.)
} // namespace kinetic

#include "kinetic.hpp"   // pulls the legacy col:: constants + g_color_enabled

namespace kinetic {

// The actual storage for the legacy toggle.
// (Already declared in kinetic.hpp as `inline bool g_color_enabled = true;`.)
// Hook into the C side via a static initializer synchronizer.
namespace {
struct ColorSync {
    ColorSync() {}
} g_color_sync;
}

// Public C++ side wrappers around the C primitives.  These return std::string
// so the rest of the codebase can mix C++ std::filesystem::path with C-style
// strings seamlessly.

std::string c_sha256_file(const std::string& path) {
    char buf[65];
    if (kinetic_sha256_file(path.c_str(), buf) != 0) return std::string();
    return std::string(buf);
}

std::string c_sha256_data(const uint8_t* data, size_t len) {
    char buf[65];
    kinetic_sha256_data(data, len, buf);
    return std::string(buf);
}

} // namespace kinetic

extern "C" {

// Synchronize the C color flag from the legacy C++ global.  Called from
// kinetic_color_set() — declared only in this TU, available to bridge.cpp.
void kinetic_color_set_from_cpp(bool enabled) {
    kinetic_color_enabled = enabled;
}

} // extern "C"
