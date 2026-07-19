/*
 * ============================================================================
 *  error/error_c.c  —  Error collector + structured printer (C side).
 *  Carries the same hint table + visual layout as the previous C++ impl.
 * ============================================================================
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "kinetic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────
 *  Hint table (sorted by code string for binary-search-style readability).
 * ──────────────────────────────────────────────────────────────────── */
typedef struct {
    const char *code;
    const char *hint;
} hint_entry;

static const hint_entry HINT_TABLE[] = {
    /* ENV */
    { "ENV_001", "Place android-sdk/ directly in $HOME, or use --sdk <path> to override." },
    { "ENV_002", "Place android-ndk/ (or android-ndk-r26c, etc.) directly in $HOME, or use --ndk <path>." },
    { "ENV_003", "Run: sdkmanager 'build-tools;34.0.0' to install build tools." },
    { "ENV_004", "Install build-tools via sdkmanager: sdkmanager 'build-tools;34.0.0'." },
    { "ENV_005", "Reinstall build-tools or check that aapt2 exists in build-tools/<ver>/aapt2." },
    { "ENV_006", "Install via Termux: pkg install aapt2   or use --aapt2 <path> to override." },
    { "ENV_007", "Install build-tools 28+: sdkmanager 'build-tools;34.0.0'  (d8 requires 28+)." },
    { "ENV_008", "Install build-tools 30+: sdkmanager 'build-tools;34.0.0'  (apksigner requires 30+)." },
    { "ENV_009", "Install build-tools: sdkmanager 'build-tools;34.0.0'." },
    { "ENV_010", "Check NDK toolchains/llvm/prebuilt/ directory exists and is not corrupted." },
    /* CFG */
    { "CFG_001", "Create kinetic.cmake in your project root. See 'kinetic --help' for reference." },
    { "CFG_002", "Fix the syntax error in kinetic.cmake. Use 'set(KINETIC_VAR value)' format." },
    { "CFG_003", "Add the missing KINETIC_ variable to kinetic.cmake. See --help for full reference." },
    { "CFG_004", "Valid ABI filters: arm64-v8a, armeabi-v7a, x86_64, x86." },
    { "CFG_005", "Ensure KINETIC_MIN_SDK and KINETIC_TARGET_SDK are positive integers." },
    { "CFG_006", "Check disk space and permissions for the output directory." },
    /* RES */
    { "RES_001", "Create src/main/res/ directory with at least values/strings.xml." },
    { "RES_002", "Fix the XML error in the indicated resource file and re-run." },
    { "RES_003", "Validate your XML with: xmllint --noout <file>." },
    { "RES_004", "Remove the duplicate resource definition from the indicated file." },
    { "RES_010", "Check AndroidManifest.xml is valid and all resource IDs are defined." },
    { "RES_011", "aapt2 link failed to produce resources.aprs — check aapt2 stderr above." },
    /* NDK */
    { "NDK_001", "Check NDK path is valid: kinetic --env." },
    { "NDK_002", "Verify the source file path in KINETIC_SOURCES matches the actual file location." },
    { "NDK_003", "Fix the compilation error shown above. Check includes and API level compatibility." },
    { "NDK_004", "Add the missing library to KINETIC_LINK_LIBS in kinetic.cmake." },
    { "NDK_005", "Add the include path to KINETIC_INCLUDE_DIRS in kinetic.cmake." },
    { "NDK_042", "Add the missing source file to KINETIC_SOURCES in kinetic.cmake." },
    /* JVM */
    { "JVM_001", "Install Java: apt install openjdk-17-jdk  or ensure javac is on PATH." },
    { "JVM_002", "Fix the Java compilation error shown above." },
    { "JVM_003", "Install Kotlin: snap install kotlin  or download from kotlinlang.org." },
    { "JVM_004", "Fix the Kotlin compilation error shown above." },
    /* DEX */
    { "DEX_001", "Check d8 is present: kinetic --env." },
    { "DEX_002", "Ensure Java/Kotlin sources compiled successfully before dex conversion." },
    { "DEX_003", "Lower KINETIC_MIN_SDK or add desugaring dependencies." },
    /* CPY */
    { "CPY_001", "Ensure the file exists at the indicated path before building." },
    { "CPY_002", "Check disk space (df -h) and directory permissions." },
    { "CPY_003", "The copied file is corrupt. Check disk health and free space." },
    { "CPY_004", "Place .so files in libs/<abi>/ matching KINETIC_ABI_FILTERS." },
    { "CPY_005", "Create src/main/assets/ directory (can be empty)." },
    /* PKG */
    { "PKG_001", "Check aapt2 (aarch64) is installed: pkg install aapt2." },
    { "PKG_002", "Create src/main/AndroidManifest.xml with a valid manifest element." },
    { "PKG_003", "Ensure AndroidManifest.xml has <manifest> root with package attribute." },
    { "PKG_004", "Check aapt2 output above for packaging errors." },
    /* SGN */
    { "SGN_001", "Generate a debug keystore: keytool -genkeypair -v -keystore ~/.android/debug.keystore "
                 "-alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -storepass android -keypass android" },
    { "SGN_002", "Check KINETIC_KEY_PASSWORD and KINETIC_STORE_PASSWORD in kinetic.cmake." },
    { "SGN_003", "Check zipalign is installed: kinetic --env." },
    { "SGN_004", "Regenerate your signing certificate — it has expired." },
};

static const size_t HINT_TABLE_LEN =
    sizeof(HINT_TABLE) / sizeof(HINT_TABLE[0]);

const char *kinetic_hint_for_code(const char *code)
{
    if (code == NULL) return NULL;
    for (size_t i = 0; i < HINT_TABLE_LEN; ++i) {
        if (strcmp(HINT_TABLE[i].code, code) == 0) return HINT_TABLE[i].hint;
    }
    return NULL;
}

/* ──────────────────────────────────────────────────────────────────────
 *  Collector lifecycle + append.
 * ──────────────────────────────────────────────────────────────────── */
void kinetic_ec_init(kinetic_error_collector_t *ec)
{
    ec->items = NULL;
    ec->count = 0;
    ec->cap   = 0;
}

static char *dup_or_empty(const char *s)
{
    if (s == NULL) s = "";
    char *r = strdup(s);
    return r ? r : strdup("");
}

int kinetic_ec_add(kinetic_error_collector_t *ec,
                   const char *phase,
                   const char *code,
                   const char *short_desc,
                   const char *file,
                   int         line,
                   int         col,
                   const char *detail,
                   const char *hint)
{
    if (ec == NULL) return -1;

    if (ec->count == ec->cap) {
        size_t ncap  = (ec->cap == 0) ? 8 : ec->cap * 2;
        kinetic_error_t *nb = (kinetic_error_t *)realloc(ec->items, ncap * sizeof(*nb));
        if (nb == NULL) return -1;
        ec->items = nb;
        ec->cap   = ncap;
    }

    kinetic_error_t *e = &ec->items[ec->count];
    e->phase      = dup_or_empty(phase);
    e->code       = dup_or_empty(code);
    e->short_desc = dup_or_empty(short_desc);
    e->file       = (file != NULL && file[0] != '\0') ? strdup(file) : NULL;
    e->line       = line;
    e->col        = col;
    e->detail     = (detail != NULL && detail[0] != '\0') ? strdup(detail) : NULL;
    e->hint       = (hint != NULL && hint[0] != '\0') ? strdup(hint) : NULL;

    ++ec->count;
    return 0;
}

void kinetic_ec_free(kinetic_error_collector_t *ec)
{
    if (ec == NULL) return;
    for (size_t i = 0; i < ec->count; ++i) {
        free(ec->items[i].phase);
        free(ec->items[i].code);
        free(ec->items[i].short_desc);
        free(ec->items[i].file);
        free(ec->items[i].detail);
        free(ec->items[i].hint);
    }
    free(ec->items);
    ec->items = NULL; ec->count = 0; ec->cap = 0;
}

/* ──────────────────────────────────────────────────────────────────────
 *  Pretty-print one error block, identical layout to the previous C++ one.
 * ──────────────────────────────────────────────────────────────────── */
static void print_indented_multiline(const char *prefix,
                                     const char *label_num_spaces,
                                     const char *ansi,
                                     const char *body)
{
    /* Each continuation line uses label_num_spaces leading spaces. */
    FILE *out = stderr;
    const char *p = body;
    bool first = true;
    while (p != NULL && *p != '\0') {
        const char *nl = strchr(p, '\n');
        size_t      ln = nl ? (size_t)(nl - p) : strlen(p);

        if (first) {
            if (kinetic_color_enabled)
                fprintf(out, "%s%s%s%s%.*s%s\n", KIN_ANSI_DIM, prefix, KIN_ANSI_RESET, ansi, (int)ln, p, KIN_ANSI_RESET);
            else
                fprintf(out, "%s%.*s\n", prefix, (int)ln, p);
            first = false;
        } else {
            if (kinetic_color_enabled)
                fprintf(out, "%s%.*s%s\n", label_num_spaces, (int)ln, p, KIN_ANSI_RESET);
            else
                fprintf(out, "%s%.*s\n", label_num_spaces, (int)ln, p);
        }
        if (nl == NULL) break;
        p = nl + 1;
    }
}

void kinetic_error_print(const kinetic_error_t *e)
{
    if (e == NULL) return;
    FILE *out = stderr;
    fprintf(out, "\n");

    const char *hint = e->hint;
    const char *hint_fallback =
        (hint == NULL || hint[0] == '\0') ? kinetic_hint_for_code(e->code) : hint;

    /* Header line */
    if (kinetic_color_enabled) {
        fprintf(out, "%s✗ [%s] %s: %s%s\n",
                KIN_ANSI_BRED,
                e->phase != NULL ? e->phase : "",
                e->code  != NULL ? e->code  : "",
                e->short_desc != NULL ? e->short_desc : "",
                KIN_ANSI_RESET);
    } else {
        fprintf(out, "✗ [%s] %s: %s\n",
                e->phase != NULL ? e->phase : "",
                e->code  != NULL ? e->code  : "",
                e->short_desc != NULL ? e->short_desc : "");
    }

    /* File */
    if (e->file != NULL && e->file[0] != '\0') {
        if (kinetic_color_enabled)
            fprintf(out, "%s  File   : %s%s\n", KIN_ANSI_DIM, KIN_ANSI_RESET, e->file);
        else
            fprintf(out, "  File   : %s\n", e->file);
    }
    /* Line / col */
    if (e->line >= 0) {
        if (kinetic_color_enabled)
            fprintf(out, "%s  Line   : %s%d", KIN_ANSI_DIM, KIN_ANSI_RESET, e->line);
        else
            fprintf(out, "  Line   : %d", e->line);
        if (e->col >= 0)
            fprintf(out, "  Col: %d", e->col);
        fprintf(out, "\n");
    }
    /* Detail */
    if (e->detail != NULL && e->detail[0] != '\0') {
        print_indented_multiline("  Detail : ",
                                 "           ",
                                 "",
                                 e->detail);
    }
    /* Hint */
    if (hint_fallback != NULL && hint_fallback[0] != '\0') {
        if (kinetic_color_enabled)
            fprintf(out, "%s  Hint   : %s%s\n", KIN_ANSI_YELLOW, hint_fallback, KIN_ANSI_RESET);
        else
            fprintf(out, "  Hint   : %s\n", hint_fallback);
    }
    /* Docs */
    if (kinetic_color_enabled)
        fprintf(out, "%s  Docs   : %shttps://kinetic.dev/errors/%s\n",
                KIN_ANSI_DIM, KIN_ANSI_RESET,
                e->code != NULL ? e->code : "");
    else
        fprintf(out, "  Docs   : https://kinetic.dev/errors/%s\n",
                e->code != NULL ? e->code : "");

    fprintf(out, "\n");
}

void kinetic_error_print_all(kinetic_error_collector_t *ec)
{
    if (ec == NULL) return;
    for (size_t i = 0; i < ec->count; ++i) {
        kinetic_error_print(&ec->items[i]);
    }
    kinetic_ec_free(ec);
    kinetic_ec_init(ec);
}

/* ──────────────────────────────────────────────────────────────────────
 *  phase_log / phase_warn / phase_fatal
 * ──────────────────────────────────────────────────────────────────── */
static char pad_phase_out[16];
static const char *pad_phase(const char *phase)
{
    if (phase == NULL) phase = "";
    size_t i = 0;
    while (i < 12 && phase[i] != '\0') { pad_phase_out[i] = phase[i]; ++i; }
    while (i < 12) { pad_phase_out[i] = ' '; ++i; }
    pad_phase_out[i] = '\0';
    return pad_phase_out;
}

void kinetic_phase_log(const char *phase, const char *msg)
{
    if (kinetic_color_enabled)
        fprintf(stdout, "%s[%s]%s  %s\n",
                KIN_ANSI_CYAN, pad_phase(phase), KIN_ANSI_RESET,
                msg != NULL ? msg : "");
    else
        fprintf(stdout, "[%s]  %s\n", pad_phase(phase), msg != NULL ? msg : "");
    fflush(stdout);
}

void kinetic_phase_warn(const char *phase, const char *msg)
{
    if (kinetic_color_enabled)
        fprintf(stdout, "%s[%s]%s  %sWARN %s%s\n",
                KIN_ANSI_BYELLOW, pad_phase(phase), KIN_ANSI_RESET,
                KIN_ANSI_YELLOW, msg != NULL ? msg : "", KIN_ANSI_RESET);
    else
        fprintf(stdout, "[%s]  WARN %s\n", pad_phase(phase), msg != NULL ? msg : "");
    fflush(stdout);
}

void kinetic_phase_fatal(const char *phase,
                         const char *code,
                         const char *short_desc,
                         const char *file,
                         const char *detail)
{
    kinetic_error_t e;
    e.phase      = (char *)(phase      != NULL ? phase      : "");
    e.code       = (char *)(code       != NULL ? code       : "");
    e.short_desc = (char *)(short_desc != NULL ? short_desc : "");
    e.file       = (char *)file;
    e.line       = -1;
    e.col        = -1;
    e.detail     = (char *)detail;
    e.hint       = NULL;
    kinetic_error_print(&e);
    /* Caller (C++ layer) is expected to throw after this returns. */
}
