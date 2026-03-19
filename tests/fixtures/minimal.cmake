# ───────────────────────────────────────────────────────────────────────────
# fixtures/minimal.cmake  —  Minimal valid kinetic.cmake for unit tests
# ───────────────────────────────────────────────────────────────────────────

set(KINETIC_APP_NAME      "TestApp")
set(KINETIC_PACKAGE_NAME  "com.test.minimal")
set(KINETIC_VERSION_CODE  1)
set(KINETIC_VERSION_NAME  "1.0.0")

set(KINETIC_MIN_SDK     21)
set(KINETIC_TARGET_SDK  34)
set(KINETIC_COMPILE_SDK 34)

set(KINETIC_ABI_FILTERS arm64-v8a)

set(KINETIC_CPP_STANDARD 17)
set(KINETIC_CPP_FLAGS    "-O2 -Wall")

set(KINETIC_KEYSTORE      "~/.android/debug.keystore")
set(KINETIC_KEY_ALIAS     "androiddebugkey")
set(KINETIC_KEY_PASSWORD  "android")
set(KINETIC_STORE_PASSWORD "android")

set(KINETIC_OUTPUT_DIR   "build/outputs")
set(KINETIC_OUTPUT_NAME  "testapp-debug")

set(KINETIC_TELEMETRY    ON)
set(KINETIC_COLOR_OUTPUT ON)
