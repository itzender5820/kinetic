// ─────────────────────────────────────────────────────────────────────────────
//  tests/test_file_copier.cpp  —  Unit tests for file_copier
// ─────────────────────────────────────────────────────────────────────────────
#include "../src/copy/file_copier.hpp"
#include "../src/utils/sha256.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace kinetic;

static int passed = 0, failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { ++passed; std::cout << "  PASS  " << msg << "\n"; } \
         else      { ++failed; std::cerr << "  FAIL  " << msg << "\n"; } } while(0)

static void test_copy_validated() {
    std::cout << "\n[copy_file_validated]\n";

    fs::path src = "/tmp/kinetic_src_test.txt";
    fs::path dst = "/tmp/kinetic_dst_dir/copied.txt";

    {
        std::ofstream f(src);
        f << "Kinetic test content 12345\n";
    }

    fs::remove_all("/tmp/kinetic_dst_dir");

    // Should succeed
    bool threw = false;
    try {
        copy_file_validated(src, dst, /*verbose=*/false, /*validate_sha=*/true, "TEST");
    } catch (...) { threw = true; }
    CHECK(!threw,                "copy_file_validated succeeds");
    CHECK(fs::exists(dst),       "destination file exists after copy");

    // SHA must match
    auto h1 = sha256_file(src);
    auto h2 = sha256_file(dst);
    CHECK(h1 == h2 && !h1.empty(), "source and dest hashes match");

    // Missing source should throw
    bool threw2 = false;
    try {
        copy_file_validated("/tmp/__nonexistent_src__", dst, false, false, "TEST");
    } catch (...) { threw2 = true; }
    CHECK(threw2, "copy_file_validated throws on missing source");

    fs::remove_all("/tmp/kinetic_dst_dir");
}

static void test_copy_dir_recursive() {
    std::cout << "\n[copy_dir_recursive]\n";

    fs::path src_dir = "/tmp/kinetic_src_dir";
    fs::path dst_dir = "/tmp/kinetic_dst_dir2";
    fs::remove_all(src_dir);
    fs::remove_all(dst_dir);
    fs::create_directories(src_dir / "sub");

    {
        std::ofstream(src_dir / "file1.txt") << "one";
        std::ofstream(src_dir / "file2.txt") << "two";
        std::ofstream(src_dir / "sub" / "file3.txt") << "three";
    }

    int n = copy_dir_recursive(src_dir, dst_dir, /*verbose=*/false, "TEST");
    CHECK(n == 3, "copy_dir_recursive copies 3 files");
    CHECK(fs::exists(dst_dir / "file1.txt"),       "file1.txt copied");
    CHECK(fs::exists(dst_dir / "file2.txt"),       "file2.txt copied");
    CHECK(fs::exists(dst_dir / "sub" / "file3.txt"), "sub/file3.txt copied");

    fs::remove_all(src_dir);
    fs::remove_all(dst_dir);
}

int main() {
    std::cout << "╔══════════════════════════════════════╗\n"
              << "║   Kinetic FileCopier Unit Tests      ║\n"
              << "╚══════════════════════════════════════╝\n";

    test_copy_validated();
    test_copy_dir_recursive();

    std::cout << "\n────────────────────────────────────────\n"
              << "  Results: " << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}
