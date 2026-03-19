# ⚡ Kinetic — Android Build System

> *"Pack everything before the developer leaves their chair for a break."*

Kinetic is a **standalone native Android build system** written in C++.  
No JVM. No Gradle. No daemon. One binary, invoked from your terminal.

---

## Table of Contents

1. [What Kinetic Does](#what-kinetic-does)
2. [Requirements](#requirements)
3. [Installing on Termux (Android)](#installing-on-termux-android)
4. [Installing on Linux (Desktop)](#installing-on-linux-desktop)
5. [SDK & NDK Setup](#sdk--ndk-setup)
6. [Building Kinetic from Source](#building-kinetic-from-source)
7. [Deploying the Binary](#deploying-the-binary)
8. [Your First Project](#your-first-project)
9. [kinetic.cmake Reference](#kineticcmake-reference)
10. [Build Pipeline (14 Phases)](#build-pipeline-14-phases)
11. [Command Reference](#command-reference)
12. [How It Works Internally](#how-it-works-internally)
13. [Troubleshooting](#troubleshooting)

---

## What Kinetic Does

Kinetic builds Android APKs from C++, Java, and Kotlin sources. It replaces
Gradle for projects that need fast, transparent, scriptable builds.

| Feature | Detail |
|---|---|
| Cold start | < 50 ms (native binary, no JVM) |
| Languages | C++20, Java, Kotlin |
| Architecture | aarch64 (arm64-v8a), armeabi-v7a, x86_64 |
| Hosts | Android/Termux (aarch64), Linux x86_64 |
| AAPT2 | Bundled aarch64 binary — no `pkg install aapt2` needed |
| Kotlin stdlib | Auto-discovered and dexed into APK automatically |
| 16KB pages | `-Wl,-z,max-page-size=16384` applied to all .so files |
| Error messages | Structured, phase-tagged, actionable |
| Telemetry | Built-in phase timing table on every build |

---

## Requirements

### On Termux (Android device)

```
pkg install clang cmake git openjdk-21 kotlin
```

| Tool | Version | Install |
|---|---|---|
| clang++ | any | `pkg install clang` |
| cmake | 3.16+ | `pkg install cmake` |
| git | any | `pkg install git` |
| javac | 11+ | `pkg install openjdk-21` |
| kotlinc | 1.9+ | `pkg install kotlin` |

### On Linux (desktop build machine)

```
sudo apt install build-essential cmake openjdk-21-jdk kotlin git
```

---

## Installing on Termux (Android)

### Step 1 — Install prerequisites

```bash
pkg update
pkg install clang cmake git openjdk-21 kotlin
```

### Step 2 — Clone and build Kinetic

```bash
cd ~
git clone https://github.com/yourname/kinetic   # or extract the tar.gz
cd kinetic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The compiled binary lands at `dist/kinetic` with `dist/assets/aapt2-aarch64` beside it.

### Step 3 — Deploy to PATH

```bash
bash deploy.sh
```

This copies both `kinetic` and `assets/aapt2-aarch64` to
`$PREFIX/bin/` and `$PREFIX/bin/assets/` so Kinetic can find its bundled
aapt2 no matter what directory you run it from.

Verify:
```bash
kinetic --version
kinetic --env
```

---

## Installing on Linux (Desktop)

```bash
cd kinetic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build     # installs to /usr/local/bin by default
```

Or without sudo:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j$(nproc)
cmake --install build
export PATH="$HOME/.local/bin:$PATH"
```

---

## SDK & NDK Setup

Kinetic auto-discovers the SDK and NDK by scanning `$HOME`.
**No environment variables need to be set** — just place the folders in the
right locations.

### Android SDK

```
$HOME/
└── android-sdk/            ← SDK root (this exact name preferred)
    ├── build-tools/
    │   └── 34.0.0/
    │       ├── aapt2
    │       ├── d8
    │       ├── apksigner
    │       └── zipalign
    ├── platforms/
    │   └── android-34/
    │       └── android.jar
    └── platform-tools/
        └── adb
```

**How to get the SDK on Termux:**

```bash
# Option A: Download SDK command-line tools from developer.android.com/studio
# and extract to ~/android-sdk/

# Option B: Use sdkmanager
cd ~/android-sdk
./cmdline-tools/latest/bin/sdkmanager "build-tools;34.0.0" "platforms;android-34"
```

Kinetic also searches `$HOME/Android/Sdk` (Android Studio default).

### Android NDK

```
$HOME/
└── android-ndk/            ← NDK root (or android-ndk-r26c, etc.)
    ├── toolchains/
    │   └── llvm/
    │       └── prebuilt/
    │           └── linux-aarch64/   ← host platform
    │               ├── bin/
    │               │   └── aarch64-linux-android24-clang++
    │               └── sysroot/
    └── build/
```

**How to get the NDK on Termux:**

```bash
# Download NDK r26c (latest stable) from developer.android.com/ndk/downloads
# Extract to ~/android-ndk-r26c/ — Kinetic finds it automatically

cd ~
wget https://dl.google.com/android/repository/android-ndk-r26c-linux.zip
unzip android-ndk-r26c-linux.zip
# Now at ~/android-ndk-r26c/ — Kinetic discovers it
```

Kinetic accepts any NDK version from r21 onwards. It selects the highest
version found under `$HOME`.

### Folder layout summary

```
$HOME/
├── android-sdk/           ← Android SDK
│   ├── build-tools/
│   ├── platforms/
│   └── platform-tools/
│
├── android-ndk/           ← Android NDK (any version name)
│   └── toolchains/llvm/
│
└── myapp/                 ← Your project
    ├── kinetic            ← kinetic binary (symlink or copy)
    ├── kinetic.cmake      ← build config
    └── src/
```

---

## Building Kinetic from Source

```bash
git clone https://github.com/yourname/kinetic
cd kinetic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

After building, `dist/` contains:
```
dist/
├── kinetic              ← the binary
└── assets/
    └── aapt2-aarch64    ← bundled aarch64 AAPT2 (no install needed)
```

**The `assets/` folder must always be next to the `kinetic` binary.**
`deploy.sh` handles this automatically.

---

## Deploying the Binary

```bash
bash deploy.sh                 # auto-detects Termux ($PREFIX/bin) or ~/bin
bash deploy.sh /usr/local/bin  # custom prefix
```

What deploy.sh does:
1. Copies `dist/kinetic` → `<prefix>/kinetic`
2. Copies `dist/assets/aapt2-aarch64` → `<prefix>/assets/aapt2-aarch64`
3. Adds `<prefix>` to PATH if not already present

---

## Your First Project

### 1. Create project structure

```
myapp/
├── kinetic.cmake
└── src/main/
    ├── AndroidManifest.xml
    ├── cpp/
    │   └── main.cpp
    ├── java/com/example/myapp/
    │   └── MainActivity.java
    ├── kotlin/com/example/myapp/
    │   └── App.kt
    └── res/values/
        └── strings.xml
```

### 2. Write kinetic.cmake

```cmake
set(KINETIC_APP_NAME        "MyApp")
set(KINETIC_PACKAGE_NAME    "com.example.myapp")
set(KINETIC_VERSION_CODE    1)
set(KINETIC_VERSION_NAME    "1.0.0")

set(KINETIC_MIN_SDK         24)
set(KINETIC_TARGET_SDK      34)
set(KINETIC_COMPILE_SDK     34)

set(KINETIC_ABI_FILTERS     arm64-v8a)

set(KINETIC_CPP_STANDARD    20)
set(KINETIC_CPP_FLAGS       "-O2 -Wall -fPIC")

set(KINETIC_SOURCES         src/main/cpp/main.cpp)
set(KINETIC_LINK_LIBS       android log)

set(KINETIC_JAVA_SOURCES    src/main/java)
set(KINETIC_KOTLIN_SOURCES  src/main/kotlin)

set(KINETIC_KEYSTORE        "~/.android/debug.keystore")
set(KINETIC_KEY_ALIAS       "androiddebugkey")
set(KINETIC_KEY_PASSWORD    "android")
set(KINETIC_STORE_PASSWORD  "android")

set(KINETIC_OUTPUT_DIR      build/outputs)
set(KINETIC_OUTPUT_NAME     "myapp-debug")
set(KINETIC_TELEMETRY       ON)
set(KINETIC_COLOR_OUTPUT    ON)
```

### 3. Build

```bash
cd myapp
kinetic --build
# Output: build/outputs/myapp-debug-signed.apk
```

### 4. Install & run on device

```bash
kinetic --install    # build + adb install
kinetic --run        # build + adb install + launch
```

---

## kinetic.cmake Reference

| Variable | Required | Default | Description |
|---|---|---|---|
| `KINETIC_APP_NAME` | no | `MyApplication` | Human-readable app name |
| `KINETIC_PACKAGE_NAME` | **yes** | — | Java package (e.g. `com.example.app`) |
| `KINETIC_VERSION_CODE` | no | `1` | Integer version for Play Store |
| `KINETIC_VERSION_NAME` | no | `1.0.0` | String version shown in Settings |
| `KINETIC_MIN_SDK` | no | `21` | Minimum Android API level |
| `KINETIC_TARGET_SDK` | no | `34` | Target Android API level |
| `KINETIC_COMPILE_SDK` | no | `34` | API level to compile against |
| `KINETIC_BUILD_TOOLS_VER` | no | auto | e.g. `34.0.0` |
| `KINETIC_ABI_FILTERS` | no | `arm64-v8a` | Space-separated ABI list |
| `KINETIC_CPP_STANDARD` | no | `17` | C++ standard (17 or 20) |
| `KINETIC_CPP_FLAGS` | no | `-O2 -Wall` | Extra clang++ flags |
| `KINETIC_SOURCES` | no | — | C++/C source files |
| `KINETIC_INCLUDE_DIRS` | no | — | Extra include directories |
| `KINETIC_LINK_LIBS` | no | — | NDK system libs (android, log, EGL…) |
| `KINETIC_PREBUILT_LIBS` | no | — | Pre-built .so files to bundle |
| `KINETIC_JAVA_SOURCES` | no | `src/main/java` | Java source root |
| `KINETIC_KOTLIN_SOURCES` | no | `src/main/kotlin` | Kotlin source root |
| `KINETIC_KEYSTORE` | no | `~/.android/debug.keystore` | Path to signing keystore |
| `KINETIC_KEY_ALIAS` | no | `androiddebugkey` | Key alias in keystore |
| `KINETIC_KEY_PASSWORD` | no | `android` | Key password |
| `KINETIC_STORE_PASSWORD` | no | `android` | Keystore password |
| `KINETIC_OUTPUT_DIR` | no | `build/outputs` | Where to write the APK |
| `KINETIC_OUTPUT_NAME` | no | `app-debug` | APK filename (without .apk) |
| `KINETIC_TELEMETRY` | no | `ON` | Show build report table |
| `KINETIC_VERBOSE_COPY` | no | `ON` | Log every file copy |
| `KINETIC_COLOR_OUTPUT` | no | `ON` | ANSI colour in terminal |
| `KINETIC_EXTRA_ASSETS` | no | — | Extra files to bundle in assets/ |

---

## Build Pipeline (14 Phases)

Kinetic executes these phases in strict order, each independently timed:

```
[01] ENV_SCAN         Discover SDK, NDK, AAPT2, javac, kotlinc, kotlin-stdlib
[02] CMAKE_PARSE      Read and validate kinetic.cmake
[03] RES_COMPILE      Compile res/ → .flat files (SDK AAPT2, no --legacy for XML)
[04] LINK_APRS        Link .flat → resources.aprs
[05] NDK_COMPILE      Compile C++ with clang++ (-fPIC, -Wl,-z,max-page-size=16384)
[06] JAVA_COMPILE     Compile .java → .class (javac)
[07] KOTLIN_COMPILE   Compile .kt → .class (kotlinc -J-Djansi.passthrough=true)
[08] DEX_CONVERT      d8: .class + kotlin-stdlib.jar → classes.dex
[09] SO_COPY          Stage .so files with ABI mapping + SHA-256 validation
[10] ASSET_COPY       Copy assets/, res/raw/, font files, libc++_shared.so
[11] MANIFEST_MERGE   Validate + stage AndroidManifest.xml
[12] APK_PACK         Package APK (aarch64 AAPT2 + zip for dex/libs/assets)
[13] SIGN_ALIGN       zipalign + apksigner → single *-signed.apk (intermediates deleted)
[14] TELEMETRY        Print phase breakdown + total time
```

### Output files

After a successful build, `build/outputs/` contains **exactly one file**:
```
build/outputs/myapp-debug-signed.apk
```
All intermediate files (`*-aligned.apk`, unsigned `*.apk`) are deleted automatically.

---

## Command Reference

```
./kinetic                          Full debug build (default)
./kinetic --build                  Same as above
./kinetic --release                Release build (optimized + signed)
./kinetic --clean                  Remove build/ directory
./kinetic --rebuild                Clean + full build
./kinetic --install                Build + adb install
./kinetic --run                    Build + install + launch app
./kinetic --check                  Validate kinetic.cmake only
./kinetic --env                    Show discovered SDK/NDK/tool paths
./kinetic --version                Print Kinetic version + build date

Build options:
  --abi arm64-v8a                  Build for one ABI only
  --no-dex                         Skip DEX conversion (C++-only apps)
  --no-sign                        Skip APK signing
  --jobs 4                         Parallel compile jobs

Output options:
  --verbose                        Print every command + file operation
  --quiet                          Suppress per-file output
  --no-color                       Disable ANSI colours
  --log build.log                  Write full log to file

Override discovery:
  --sdk /path/to/sdk               Override auto-discovered SDK
  --ndk /path/to/ndk               Override auto-discovered NDK
  --aapt2 /path/to/aapt2           Override system AAPT2 path (file or directory)
```

---

## How It Works Internally

### AAPT2 Dual Strategy

Kinetic uses two AAPT2 binaries:

| Phase | Binary | Why |
|---|---|---|
| RES_COMPILE, LINK_APRS | Bundled `assets/aapt2-aarch64` | SDK aapt2 is x86_64 and crashes on ARM64 devices |
| APK_PACK | Same bundled binary | Consistent behaviour |

On x86_64 build machines the SDK aapt2 is used for all phases.

### 16KB Page Support

All `.so` files compiled by Kinetic include:
```
-Wl,-z,max-page-size=16384
```
This aligns ELF LOAD segments to 16KB boundaries, which is required for
Android 15+ devices running on 16KB page-size kernels. The flag is harmless
on traditional 4KB-page devices.

### Kotlin stdlib bundling

Kotlin runtime classes (`kotlin.jvm.internal.Intrinsics`, `kotlin.collections.*`,
etc.) are not present on Android devices. They must be inside the APK.

Kinetic's DEX phase passes `kotlin-stdlib.jar` as a **positional input** to d8:
```
d8 --min-api 24 --lib android.jar  \
   kotlin-stdlib.jar               \  ← bundled into classes.dex
   com/example/app/MainActivity.class ...
```
Without this, the app crashes on first launch with:
```
NoClassDefFoundError: Lkotlin/jvm/internal/Intrinsics;
```

### Path discovery (no hardcoded paths)

All tool paths are resolved from environment variables:
- `$HOME` — SDK, NDK, project root
- `$PREFIX` — Termux tools (`$PREFIX/bin/aapt2`, `$PREFIX/lib/kotlinc/`)
- `$JAVA_HOME` — alternative kotlinc stdlib location
- `$PATH` — fallback for javac, kotlinc, aapt2, adb

No path in Kinetic is hardcoded to a specific user or system layout.

---

## Troubleshooting

### `ENV_006: System AAPT2 not found`
Kinetic can't find an aarch64 aapt2. Make sure the `assets/` folder is next
to the `kinetic` binary. Run `bash deploy.sh` to fix this.

### `JVM_004: kotlinc — Failed to load native library jansi`
Your kotlinc binary tries to use a glibc native library. Kinetic automatically
passes `-J-Djansi.passthrough=true` to suppress this. If you see it, you are
running an old kinetic binary — rebuild or redeploy.

### `NoClassDefFoundError: Lkotlin/jvm/internal/Intrinsics`
The Kotlin stdlib was not bundled. Kinetic will warn during ENV_SCAN:
```
WARN kotlin-stdlib.jar not found
```
Check that kotlinc is on PATH and `$PREFIX/lib/kotlinc/lib/kotlin-stdlib.jar`
exists. On Termux: `pkg install kotlin`.

### `NDK_003: Compilation failed`
- Check that `KINETIC_SOURCES` paths are correct relative to the project root
- Verify `KINETIC_INCLUDE_DIRS` lists all needed include directories
- Common fix: add `android_native_app_glue.c` to `KINETIC_SOURCES`

### App crashes on Android 15+ with SIGBUS
The `.so` was not compiled with 16KB page alignment. Rebuild — Kinetic adds
`-Wl,-z,max-page-size=16384` automatically.

### Multiple APK files in build/outputs/
Old kinetic binary. The current version outputs only `*-signed.apk`.
Run `kinetic --clean && kinetic --build` to regenerate.

---

## Architecture (Kinetic Source Tree)

```
kinetic/
├── CMakeLists.txt
├── deploy.sh
├── assets/
│   └── aapt2-aarch64          ← bundled aarch64 AAPT2
├── dist/                      ← compiled binary + assets
│   ├── kinetic
│   └── assets/
│       └── aapt2-aarch64
└── src/
    ├── main.cpp               ← entry point
    ├── kinetic.hpp            ← master header
    ├── core/                  ← engine, phase_timer, telemetry
    ├── env/                   ← env_scanner, aapt2_resolver
    ├── config/                ← cmake_parser, config_model
    ├── phases/                ← 11 build phase implementations
    ├── error/                 ← error_reporter, error codes, hints
    ├── copy/                  ← file_copier (SHA-256 validated), essential_manifest
    ├── cli/                   ← cli_parser, help_printer
    └── utils/                 ← process (fork/exec), sha256
```

---

*Kinetic v1.0.0 — Build date injected at compile time (`kinetic --version`)*
