# ═══════════════════════════════════════════════════════════════════════════════
# kinetic.cmake  —  TESTAPP  Music Player
# Pure C++20 backend · OpenGL ES 2.0 visualizer · OpenSL ES audio · Kotlin/Java UI
# ═══════════════════════════════════════════════════════════════════════════════

set(KINETIC_APP_NAME        "TESTAPP")
set(KINETIC_PACKAGE_NAME    "com.testapp")
set(KINETIC_VERSION_CODE    1)
set(KINETIC_VERSION_NAME    "1.0.0")

# ── SDK / NDK ─────────────────────────────────────────────────────────────────
set(KINETIC_MIN_SDK         24)
set(KINETIC_TARGET_SDK      34)
set(KINETIC_COMPILE_SDK     34)

# ── ABI ───────────────────────────────────────────────────────────────────────
set(KINETIC_ABI_FILTERS     arm64-v8a)

# ── C++20 backend ─────────────────────────────────────────────────────────────
set(KINETIC_CPP_STANDARD    20)
set(KINETIC_CPP_FLAGS       "-O2 -Wall -Wextra -Wno-unused-parameter -ffast-math -fPIC")

set(KINETIC_SOURCES
    src/main/cpp/audio/AudioEngine.cpp
    src/main/cpp/gl/Renderer.cpp
    src/main/cpp/bridge/JniBridge.cpp
)

set(KINETIC_INCLUDE_DIRS
    src/main/cpp/include
    src/main/cpp/audio
    src/main/cpp/gl
)

set(KINETIC_LINK_LIBS       android log OpenSLES EGL GLESv2)

# ── Java / Kotlin ─────────────────────────────────────────────────────────────
set(KINETIC_JAVA_VERSION    "11")
set(KINETIC_KOTLIN_VERSION  "2.3")
set(KINETIC_JAVA_SOURCES    src/main/java)
set(KINETIC_KOTLIN_SOURCES  src/main/kotlin)

# ── Signing ───────────────────────────────────────────────────────────────────
set(KINETIC_KEYSTORE        "~/.android/debug.keystore")
set(KINETIC_KEY_ALIAS       "androiddebugkey")
set(KINETIC_KEY_PASSWORD    "android")
set(KINETIC_STORE_PASSWORD  "android")

# ── Output ────────────────────────────────────────────────────────────────────
set(KINETIC_OUTPUT_DIR      build/outputs)
set(KINETIC_OUTPUT_NAME     "testapp-debug")

# ── Telemetry ─────────────────────────────────────────────────────────────────
set(KINETIC_TELEMETRY       ON)
set(KINETIC_VERBOSE_COPY    ON)
set(KINETIC_COLOR_OUTPUT    ON)
