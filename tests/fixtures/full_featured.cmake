# ═══════════════════════════════════════════════════════════════
# fixtures/full_featured.cmake  —  Full kinetic.cmake for tests
# ═══════════════════════════════════════════════════════════════

# ── Project Identity ─────────────────────────────────────────
set(KINETIC_APP_NAME        "MyApplication")
set(KINETIC_PACKAGE_NAME    "com.example.myapp")
set(KINETIC_VERSION_CODE    1)
set(KINETIC_VERSION_NAME    "1.0.0")

# ── SDK / NDK Configuration ───────────────────────────────────
set(KINETIC_MIN_SDK         24)
set(KINETIC_TARGET_SDK      34)
set(KINETIC_COMPILE_SDK     34)
set(KINETIC_BUILD_TOOLS_VER "34.0.0")

# ── ABI Targets ───────────────────────────────────────────────
set(KINETIC_ABI_FILTERS
    arm64-v8a
    armeabi-v7a
    x86_64
)

# ── Language Configuration ────────────────────────────────────
set(KINETIC_CPP_STANDARD    17)
set(KINETIC_CPP_FLAGS       "-O2 -Wall -Wextra -ffast-math")
set(KINETIC_JAVA_VERSION    "11")
set(KINETIC_KOTLIN_VERSION  "1.9")

# ── C++ Source Files ──────────────────────────────────────────
set(KINETIC_SOURCES
    src/main/cpp/main.cpp
    src/main/cpp/renderer.cpp
    src/main/cpp/audio_engine.cpp
    src/main/cpp/android_native_app_glue.c
)

# ── Include Directories ──────────────────────────────────────
set(KINETIC_INCLUDE_DIRS
    src/main/cpp/include
    src/main/cpp/third_party/glm
)

# ── Shared Libraries to Link ──────────────────────────────────
set(KINETIC_LINK_LIBS
    android log OpenSLES EGL GLESv3
)

# ── Pre-built .so Libraries ───────────────────────────────────
set(KINETIC_PREBUILT_LIBS
    libs/arm64-v8a/liboboe.so
    libs/arm64-v8a/libvulkan_wrapper.so
)

# ── Java / Kotlin Sources ────────────────────────────────────
set(KINETIC_JAVA_SOURCES    src/main/java)
set(KINETIC_KOTLIN_SOURCES  src/main/kotlin)

# ── Signing Configuration ────────────────────────────────────
set(KINETIC_KEYSTORE        "~/.android/debug.keystore")
set(KINETIC_KEY_ALIAS       "androiddebugkey")
set(KINETIC_KEY_PASSWORD    "android")
set(KINETIC_STORE_PASSWORD  "android")

# ── Output Configuration ─────────────────────────────────────
set(KINETIC_OUTPUT_DIR      build/outputs)
set(KINETIC_OUTPUT_NAME     "myapp-debug")

# ── Telemetry Options ────────────────────────────────────────
set(KINETIC_TELEMETRY       ON)
set(KINETIC_VERBOSE_COPY    ON)
set(KINETIC_COLOR_OUTPUT    ON)

# ── Extra Assets ─────────────────────────────────────────────
set(KINETIC_EXTRA_ASSETS
    raw_data/shader.vert
    raw_data/shader.frag
    config/default_settings.json
)

# ═══════════════════════════════════════════════════════════════
# End of full_featured.cmake
# ═══════════════════════════════════════════════════════════════
