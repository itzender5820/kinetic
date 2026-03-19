// ─────────────────────────────────────────────────────────────────────────────
//  tests/test_env_scanner.cpp  —  Unit tests for env discovery helpers
// ─────────────────────────────────────────────────────────────────────────────
#include "../src/utils/process.hpp"
#include "../src/utils/sha256.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstring>

using namespace kinetic;

static int passed = 0, failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { ++passed; std::cout << "  PASS  " << msg << "\n"; } \
         else      { ++failed; std::cerr << "  FAIL  " << msg << "\n"; } } while(0)

static void test_which() {
    std::cout << "\n[which()]\n";
    // 'sh' must exist on any POSIX system
    auto sh = which("sh");
    CHECK(!sh.empty(), "which(sh) returns non-empty");
    CHECK(sh.find("sh") != std::string::npos, "which(sh) contains 'sh'");

    auto missing = which("__kinetic_nonexistent_binary__");
    CHECK(missing.empty(), "which(nonexistent) returns empty");
}

static void test_expand_home() {
    std::cout << "\n[expand_home()]\n";
    setenv("HOME", "/home/testuser", 1);
    CHECK(expand_home("~/.android") == "/home/testuser/.android", "expand_home tilde");
    CHECK(expand_home("/absolute/path") == "/absolute/path",       "expand_home abs path unchanged");
    CHECK(expand_home("relative")      == "relative",              "expand_home relative unchanged");
}

static void test_sha256() {
    std::cout << "\n[sha256]\n";

    // Known SHA-256 of empty string
    const uint8_t empty[1] = {};
    auto h0 = sha256_data(empty, 0);
    CHECK(h0 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256 of empty");

    // Known SHA-256 of "abc" — NIST FIPS 180-4 test vector
    const uint8_t abc[] = { 'a', 'b', 'c' };
    auto habc = sha256_data(abc, 3);
    CHECK(habc == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256 of 'abc' matches NIST test vector");
    CHECK(habc.size() == 64, "sha256 output is 64 hex chars");

    // File SHA-256
    {
        std::ofstream f("/tmp/kinetic_test_sha.txt");
        f << "hello kinetic\n";
    }
    auto fhash = sha256_file("/tmp/kinetic_test_sha.txt");
    CHECK(fhash.size() == 64, "sha256_file returns 64 char hex");
    // Same file hashed twice must match
    auto fhash2 = sha256_file("/tmp/kinetic_test_sha.txt");
    CHECK(fhash == fhash2, "sha256_file is deterministic");
}

static void test_exec_proc() {
    std::cout << "\n[exec_proc()]\n";

    auto r = exec_proc({"echo", "hello kinetic"});
    CHECK(r.exit_code == 0,                              "echo exits 0");
    CHECK(r.out.find("hello kinetic") != std::string::npos, "echo output captured");

    auto r2 = exec_proc({"false"});
    CHECK(r2.exit_code != 0, "false exits non-zero");

    auto r3 = exec_proc({"ls", "/tmp"});
    CHECK(r3.exit_code == 0, "ls /tmp succeeds");
}

int main() {
    std::cout << "╔══════════════════════════════════════╗\n"
              << "║   Kinetic Env Scanner Unit Tests     ║\n"
              << "╚══════════════════════════════════════╝\n";

    test_which();
    test_expand_home();
    test_sha256();
    test_exec_proc();

    std::cout << "\n────────────────────────────────────────\n"
              << "  Results: " << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}
