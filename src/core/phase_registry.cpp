// ─────────────────────────────────────────────────────────────────────────────
//  core/phase_registry.cpp  —  Factory that registers all Phase subclasses
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_registry.hpp"
#include "../phases/phase_env_scan.hpp"
#include "../phases/phase_cmake_parse.hpp"
#include "../phases/phase_res_compile.hpp"
#include "../phases/phase_link_aprs.hpp"
#include "../phases/phase_ndk_compile.hpp"
#include "../phases/phase_java_compile.hpp"
#include "../phases/phase_kotlin_compile.hpp"
#include "../phases/phase_dex_convert.hpp"
#include "../phases/phase_so_copy.hpp"
#include "../phases/phase_asset_copy.hpp"
#include "../phases/phase_manifest_merge.hpp"
#include "../phases/phase_apk_pack.hpp"
#include "../phases/phase_sign_align.hpp"

namespace kinetic {

void register_all_phases(Dag& dag) {
    // The registration order matches the legacy phase sequence.
    // The DAG derives edges from each phase's requires() method.
    dag.add(std::make_unique<PhaseEnvScan>());
    dag.add(std::make_unique<PhaseCmakeParse>());
    dag.add(std::make_unique<PhaseResCompile>());
    dag.add(std::make_unique<PhaseLinkAprs>());
    dag.add(std::make_unique<PhaseNdkCompile>());
    dag.add(std::make_unique<PhaseJavaCompile>());
    dag.add(std::make_unique<PhaseKotlinCompile>());
    dag.add(std::make_unique<PhaseDexConvert>());
    dag.add(std::make_unique<PhaseSoCopy>());
    dag.add(std::make_unique<PhaseAssetCopy>());
    dag.add(std::make_unique<PhaseManifestMerge>());
    dag.add(std::make_unique<PhaseApkPack>());
    dag.add(std::make_unique<PhaseSignAlign>());
}

} // namespace kinetic
