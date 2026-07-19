## currunt state

╭─ [itx.ender 󰀲 localhost] [~/projects/kinetic]  main
╰─❯ ./dist/kinetic

  ⚡ KINETIC  v1.0  —  Android Build System
  ─────────────────────────────────────────────────────────────
[ENV_SCAN    ]  Scanning $HOME (/data/data/com.termux/files/home) for SDK/NDK ...
[ENV_SCAN    ]  Host arch: aarch64
[ENV_SCAN    ]  aarch64 host: native aapt2 → /data/data/com.termux/files/home/projects/kinetic/dist/assets/aapt2-aarch64
[ENV_SCAN    ]  AAPT2 → sdk:36.0.0  |  sys:aapt2-aarch64 (bundled)
[ENV_SCAN    ]  SDK   → /data/data/com.termux/files/home/android-sdk
[ENV_SCAN    ]  NDK   → /data/data/com.termux/files/home/android-sdk/ndk/28.2.13676358

✗ [CMAKE_PARSE] CFG_001: kinetic.cmake not found
  File   : /data/data/com.termux/files/home/projects/kinetic/kinetic.cmake
  Detail : Create kinetic.cmake in your project root.
           Run: kinetic --help for a full reference.
  Hint   : Create kinetic.cmake in your project root. See 'kinetic --help' for reference.
  Docs   : https://kinetic.dev/errors/CFG_001


╭─ [itx.ender 󰀲 localhost] [~/projects/kinetic]  main
╰─❯ ./dist/kinetic --help                                                                   ✘ 1
╔══════════════════════════════════════════════════════════════════╗
║          ⚡  K I N E T I C   B U I L D   S Y S T E M             ║
║               Android  •  C++  •  Java  •  Kotlin                ║
║                         Version 1.0                              ║
╠══════════════════════════════════════════════════════════════════╣
║  USAGE:  ./kinetic [command] [options]                           ║
╠══════════════════════════════════════════════════════════════════╣
║  COMMANDS                                                        ║
╠══════════════╦═══════════════════════════════════════════════════╣
║  --build     ║  Full build pipeline (default command)            ║
║  --clean     ║  Remove build/ directory entirely                 ║
║  --rebuild   ║  Clean then full build                            ║
║  --install   ║  Build + push APK via adb install                 ║
║  --run       ║  Build, install, then launch app via adb          ║
║  --check     ║  Validate kinetic.cmake only, no build            ║
║  --env       ║  Print discovered SDK/NDK/AAPT2 paths             ║
║  --version   ║  Print Kinetic version and exit                   ║
║  --help      ║  Show this help panel                             ║
╠══════════════╩═══════════════════════════════════════════════════╣
║  BUILD OPTIONS                                                   ║
╠══════════════╦═══════════════════════════════════════════════════╣
║  --release   ║  Build in release mode (optimized, signed)        ║
║  --debug     ║  Build in debug mode (default)                    ║
║  --abi <abi> ║  Build for specific ABI only                      ║
║              ║  Values: arm64-v8a | armeabi-v7a | x86_64         ║
║  --jobs <n>  ║  Parallel compile jobs (default: auto)            ║
║  --no-dex    ║  Skip Dex conversion (C++ only apps)              ║
║  --no-sign   ║  Skip APK signing step                            ║
╠══════════════╩═══════════════════════════════════════════════════╣
║  OUTPUT & LOGGING OPTIONS                                        ║
╠══════════════╦═══════════════════════════════════════════════════╣
║  --verbose   ║  Print every command and file operation           ║
║  --quiet     ║  Suppress per-file output, show summary only      ║
║  --no-color  ║  Disable ANSI color output                        ║
║  --log <f>   ║  Write full build log to file                     ║
║  --phase <n> ║  Run a single phase only (for debugging)          ║
║              ║  Values: env|cmake|res|ndk|java|kotlin|           ║
║              ║          dex|copy|pack|sign                       ║
╠══════════════╩═══════════════════════════════════════════════════╣
║  ENVIRONMENT OVERRIDES                                           ║
╠══════════════╦═══════════════════════════════════════════════════╣
║  --sdk <p>   ║  Override auto-discovered SDK path                ║
║  --ndk <p>   ║  Override auto-discovered NDK path                ║
║  --aapt2 <p> ║  Override system AAPT2 path (aarch64)             ║
╠══════════════╩═══════════════════════════════════════════════════╣
║  EXAMPLES                                                        ║
╠══════════════════════════════════════════════════════════════════╣
║  ./kinetic                            Full debug build           ║
║  ./kinetic --release                  Release build              ║
║  ./kinetic --run                      Build + install + launch   ║
║  ./kinetic --build --abi arm64-v8a    Single ABI build           ║
║  ./kinetic --build --verbose          Verbose build              ║
║  ./kinetic --env                      Check environment setup    ║
╠══════════════════════════════════════════════════════════════════╣
║  Place android-sdk/ and android-ndk/ in $HOME                    ║
║  Place kinetic.cmake in your project root                        ║
║  Install system AAPT2 via:  pkg install aapt2                    ║
╚══════════════════════════════════════════════════════════════════╝


## INTRODUCTION TO THIS TOOL IN README.md 

## your JOB
- make it support modular build
  - just like gradle can compile infinity large source to android apk
- make it support auto dependency downloding
  - same a gradle does and stores in $HOME/.gradle directory
- make it also support C alongside C++ Kotlin and java 
  - C is a crucial language for android low level application development 
- rewrite the tool in C/C++ focus on robustness alongside performance and optmization 
  - C has universal ABI and C++ will let you access modern features
- make DAG more robust 
  - currutly its a workaround level DAG implimentation
- make it support Flutter-SDK
  - so it can compile flutter source without patching
- make it compile in any enviorment bettwen Arm64 or amd64 
  - so one can compile without patching
- better error handling
  - show erroes alongside files causing them
