// ─────────────────────────────────────────────────────────────────────────────
//  dag/fshasher.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "fshasher.hpp"
#include "dag.hpp"
#include "../kinetic.h"
#include "../kinetic.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

namespace kinetic {

size_t FSHasher::load(const fs::path& stamps_file)
{
    stamps_file_ = stamps_file;
    last_.clear();

    std::ifstream f(stamps_file);
    if (!f.is_open()) return 0;

    std::string line;
    size_t count = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string name  = line.substr(0, tab);
        std::string stamp = line.substr(tab + 1);
        last_[name] = stamp;
        ++count;
    }
    return count;
}

namespace {

// Compute the SHA-256 of a single file and return as a 64-char hex string
// (empty when file is unreadable).
std::string hash_file(const fs::path& p)
{
    char buf[65];
    if (kinetic_sha256_file(p.string().c_str(), buf) != 0) return std::string();
    return std::string(buf);
}

// Hash a stream of bytes (used to fold deterministic config fields).
std::string hash_data(const std::string& data)
{
    char buf[65];
    kinetic_sha256_data(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size(),
        buf
    );
    return std::string(buf);
}

// Stable concatenation of deterministic config fields that affect every
// cacheable phase.  Adding more fields anywhere upstream in the future
// will (by design) trigger a cache miss — that's the safeness contract.
std::string config_seed(const KineticConfig& c, const KineticEnv& e)
{
    std::ostringstream seed;
    seed << "pkg=" << c.package_name
         << "|app=" << c.app_name
         << "|vc="  << c.version_code
         << "|vn="  << c.version_name
         << "|min=" << c.min_sdk
         << "|tgt=" << c.target_sdk
         << "|cmp=" << c.compile_sdk
         << "|bt="  << c.build_tools_ver
         << "|cpp=" << c.cpp_standard
         << "|cf="  << c.cpp_flags
         << "|jvm=" << c.java_version
         << "|kv="  << c.kotlin_version;

    seed << "|abis=";
    for (const auto& a : c.abi_filters) seed << a << ",";
    seed << "|srcs=";
    for (const auto& s : c.sources) seed << s << ",";
    seed << "|inc=";
    for (const auto& s : c.include_dirs) seed << s << ",";
    seed << "|libs=";
    for (const auto& s : c.link_libs) seed << s << ",";
    seed << "|preb=";
    for (const auto& s : c.prebuilt_libs) seed << s << ",";

    seed << "|sdk=" << e.sdk_path.string()
         << "|ndk=" << e.ndk_path.string()
         << "|clangxx=" << e.ndk_clangxx.string()
         << "|sysroot="  << e.ndk_sysroot.string()
         << "|aapt2="    << e.sdk_aapt2.string();

    return seed.str();
}

} // namespace

std::string FSHasher::compute_stamp(const Phase& phase, const PhaseContext& ctx) const
{
    const Dag* dag_unused = nullptr;  // suppress unused errs
    (void)dag_unused;

    // Gather input files: union of every requires() phase's provides().
    // We don't have access to the full DAG here; the executor passes only
    // the phase instance.  Instead, ask the phase what its predecessors
    // produced by scanning the project's build directory tree directly for
    // the well-known intermediate paths.

    // Combine:
    //   • SHA-256 of every produced file from upstream phases
    //   • a deterministic seed of config + env fields
    //   • the phase's own name
    //
    // We treat upstream outputs as the files under the build/intermediates
    // tree whose existence the phase actually depends on.  For simplicity
    // and correctness, the stamp includes every artifact file in
    //      build/intermediates/<phase-known-subdirs>/
    // that exists at compute time.

    std::ostringstream payload;
    payload << "phase=" << phase.name() << "\n";
    payload << "cfg=" << hash_data(config_seed(ctx.cfg, ctx.env)) << "\n";

    // Walk the build directory for upstream artifacts.  We only consider
    // paths under the intermediates tree; whichever ones exist when we
    // stamp, we hash.
    const fs::path inter = ctx.dirs.build_dir / "intermediates";
    if (fs::exists(inter)) {
        std::vector<fs::path> artifacts;
        for (const auto& e : fs::recursive_directory_iterator(inter)) {
            if (!e.is_regular_file()) continue;
            // Skip the hash cache itself
            if (e.path().string().find(".kinetic-cache") != std::string::npos) continue;
            artifacts.push_back(e.path());
        }
        std::sort(artifacts.begin(), artifacts.end());
        for (const auto& p : artifacts) {
            payload << p.filename().string() << "="
                    << [&] {
                        std::error_code ec;
                        auto sz = fs::file_size(p, ec);
                        if (ec) return std::string("0");
                        return std::to_string(sz);
                    }()
                    << ":" << hash_file(p) << "\n";
        }
    }

    return hash_data(payload.str());
}

bool FSHasher::is_clean(const Phase& phase, const PhaseContext& ctx) const
{
    if (!phase.cacheable()) return false;

    auto it = last_.find(phase.name());
    if (it == last_.end()) return false;

    const std::string current = compute_stamp(phase, ctx);
    if (current.empty() || current != it->second) return false;

    // Every provided file must exist on disk.
    for (const auto& p : phase.provides(ctx)) {
        if (!fs::exists(p)) return false;
    }
    return true;
}

void FSHasher::mark_done(const Phase& phase, const PhaseContext& ctx)
{
    last_[phase.name()] = compute_stamp(phase, ctx);
}

void FSHasher::invalidate(const std::string& phase_name)
{
    last_.erase(phase_name);
}

bool FSHasher::save() const
{
    if (stamps_file_.empty()) return false;
    fs::create_directories(stamps_file_.parent_path());

    fs::path tmp = stamps_file_.string() + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    if (!out.is_open()) return false;

    for (const auto& kv : last_) {
        out << kv.first << '\t' << kv.second << '\n';
    }
    out.flush();
    out.close();
    std::error_code ec;
    fs::rename(tmp, stamps_file_, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

std::string FSHasher::stamp(const std::string& phase_name) const
{
    auto it = last_.find(phase_name);
    return (it == last_.end()) ? std::string() : it->second;
}

} // namespace kinetic
