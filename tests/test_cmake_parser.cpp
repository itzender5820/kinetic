// ─────────────────────────────────────────────────────────────────────────────
//  tests/test_cmake_parser.cpp  —  Unit tests for CMakeParser
// ─────────────────────────────────────────────────────────────────────────────
//  Build & run:
//    g++ -std=c++17 test_cmake_parser.cpp
//        ../src/config/cmake_parser.cpp
//        ../src/config/config_model.cpp
//        ../src/error/error_reporter.cpp
//        -o test_cmake_parser && ./test_cmake_parser
// ─────────────────────────────────────────────────────────────────────────────
#include "../src/config/cmake_parser.hpp"
#include "../src/config/config_model.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace kinetic;

static int passed = 0, failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { ++passed; std::cout << "  PASS  " << msg << "\n"; } \
         else      { ++failed; std::cerr << "  FAIL  " << msg << "\n"; } } while(0)

static void test_minimal() {
    std::cout << "\n[minimal.cmake]\n";
    CMakeParser p;
    auto cfg = p.parse("fixtures/minimal.cmake", ".");

    CHECK(cfg.package_name == "com.test.minimal",    "package_name");
    CHECK(cfg.app_name     == "TestApp",             "app_name");
    CHECK(cfg.min_sdk      == 21,                    "min_sdk");
    CHECK(cfg.target_sdk   == 34,                    "target_sdk");
    CHECK(cfg.version_code == 1,                     "version_code");
    CHECK(cfg.cpp_standard == 17,                    "cpp_standard");
    CHECK(cfg.abi_filters.size() == 1,               "abi_filters count");
    CHECK(cfg.abi_filters[0] == "arm64-v8a",         "abi_filters[0]");
    CHECK(cfg.output_name  == "testapp-debug",       "output_name");
    CHECK(cfg.telemetry    == true,                  "telemetry ON");
}

static void test_full_featured() {
    std::cout << "\n[full_featured.cmake]\n";
    CMakeParser p;
    auto cfg = p.parse("fixtures/full_featured.cmake", ".");

    CHECK(cfg.package_name       == "com.example.myapp",  "package_name");
    CHECK(cfg.app_name           == "MyApplication",       "app_name");
    CHECK(cfg.min_sdk            == 24,                    "min_sdk");
    CHECK(cfg.build_tools_ver    == "34.0.0",              "build_tools_ver");
    CHECK(cfg.abi_filters.size() == 3,                     "abi_filters count");
    CHECK(cfg.abi_filters[0]     == "arm64-v8a",           "abi_filters[0]");
    CHECK(cfg.abi_filters[1]     == "armeabi-v7a",         "abi_filters[1]");
    CHECK(cfg.abi_filters[2]     == "x86_64",              "abi_filters[2]");
    CHECK(cfg.sources.size()     == 4,                     "sources count");
    CHECK(cfg.link_libs.size()   == 5,                     "link_libs count");
    CHECK(cfg.prebuilt_libs.size() == 2,                   "prebuilt_libs count");
    CHECK(cfg.extra_assets.size()  == 3,                   "extra_assets count");
    CHECK(cfg.verbose_copy       == true,                  "verbose_copy ON");
    CHECK(cfg.cpp_flags.find("-ffast-math") != std::string::npos, "cpp_flags ffast-math");
}

static void test_validate_missing_package() {
    std::cout << "\n[validate: missing package]\n";
    KineticConfig cfg;
    cfg.package_name = ""; // deliberately empty
    cfg.min_sdk      = 21;
    cfg.target_sdk   = 34;
    bool threw = false;
    try { cfg.validate(); } catch (...) { threw = true; }
    CHECK(threw, "validate() throws on empty package_name");
}

static void test_validate_bad_sdk() {
    std::cout << "\n[validate: min_sdk > target_sdk]\n";
    KineticConfig cfg;
    cfg.package_name = "com.test.x";
    cfg.min_sdk      = 35;
    cfg.target_sdk   = 21;
    bool threw = false;
    try { cfg.validate(); } catch (...) { threw = true; }
    CHECK(threw, "validate() throws when min_sdk > target_sdk");
}

static void test_validate_bad_abi() {
    std::cout << "\n[validate: invalid ABI]\n";
    KineticConfig cfg;
    cfg.package_name = "com.test.x";
    cfg.min_sdk      = 21;
    cfg.target_sdk   = 34;
    cfg.abi_filters  = { "mips64" }; // invalid
    bool threw = false;
    try { cfg.validate(); } catch (...) { threw = true; }
    CHECK(threw, "validate() throws on invalid ABI filter");
}

int main() {
    std::cout << "╔══════════════════════════════════════╗\n"
              << "║   Kinetic CMakeParser Unit Tests     ║\n"
              << "╚══════════════════════════════════════╝\n";

    test_minimal();
    test_full_featured();
    test_validate_missing_package();
    test_validate_bad_sdk();
    test_validate_bad_abi();

    std::cout << "\n────────────────────────────────────────\n"
              << "  Results: " << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}
