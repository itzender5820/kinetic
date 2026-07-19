cmake_minimum_required(VERSION 3.16)
project(kinetic VERSION 2.0.0 LANGUAGES C CXX)

# ── C standard ──────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE   "-O2 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG     "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra -Wpedantic")

# ── C sources (universal-ABI core) ──────────────────────────────────────────
set(KINETIC_C_SOURCES
    src/main.c

    src/c/color.c
    src/c/kinetic_c.c

    src/utils/sha256_c.c
    src/utils/process_c.c

    src/error/error_c.c
)

# ── C++ sources (engine, DAG, phases, config) ───────────────────────────────
set(KINETIC_CXX_SOURCES
    src/kinetic.cpp
    src/bridge.cpp

    src/utils/process.cpp
    src/utils/sha256.cpp
    src/copy/file_copier.cpp
    src/copy/essential_manifest.cpp

    src/error/error_reporter.cpp

    src/config/config_model.cpp
    src/config/cmake_parser.cpp

    src/env/env_scanner.cpp
    src/env/aapt2_resolver.cpp

    src/core/phase_timer.cpp
    src/core/telemetry.cpp
    src/core/phase_registry.cpp
    src/core/engine.cpp

    src/dag/dag.cpp
    src/dag/fshasher.cpp
    src/dag/executor.cpp

    src/phases/phase_env_scan.cpp
    src/phases/phase_cmake_parse.cpp
    src/phases/phase_res_compile.cpp
    src/phases/phase_link_aprs.cpp
    src/phases/phase_ndk_compile.cpp
    src/phases/phase_java_compile.cpp
    src/phases/phase_kotlin_compile.cpp
    src/phases/phase_dex_convert.cpp
    src/phases/phase_so_copy.cpp
    src/phases/phase_asset_copy.cpp
    src/phases/phase_manifest_merge.cpp
    src/phases/phase_apk_pack.cpp
    src/phases/phase_sign_align.cpp

    src/cli/cli_parser.cpp
    src/cli/help_printer.cpp
)

add_executable(kinetic ${KINETIC_C_SOURCES} ${KINETIC_CXX_SOURCES})

target_include_directories(kinetic PRIVATE src)

# Inject build timestamp so --version always shows when this binary was compiled
string(TIMESTAMP KINETIC_BUILD_DATE "%Y%m%d-%H%M")
target_compile_definitions(kinetic PRIVATE
    KINETIC_BUILD_DATE="${KINETIC_BUILD_DATE}"
)

# ── Platform / compiler flags ─────────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(kinetic PRIVATE KINETIC_LINUX)
endif()

# pthread is required by both the C++ DAG executor and the C process module
find_package(Threads REQUIRED)
target_link_libraries(kinetic PRIVATE Threads::Threads)

# Filesystem linking (GCC < 9 needs explicit -lstdc++fs)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(kinetic PRIVATE stdc++fs)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS kinetic DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
        DESTINATION bin/assets
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE)

# ── Output binary into dist/ ──────────────────────────────────────────────────
set_target_properties(kinetic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
)

# ── Copy assets/ into dist/ so the binary finds them next to itself ────────────
add_custom_command(TARGET kinetic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_SOURCE_DIR}/dist/assets"
    COMMAND chmod -R +x "${CMAKE_SOURCE_DIR}/dist/assets"
    COMMENT "Copying assets/ to dist/assets/ and setting +x"
)
