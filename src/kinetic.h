/*
 * ============================================================================
 *  kinetic.h  —  Public C ABI for the Kinetic build system.
 *
 *  All functions in the C layer use the `kinetic_` prefix. They are reachable
 *  from the C++ engine via `extern "C"` (see kinetic.cpp / bridge.cpp).
 *
 *  The C layer owns:
 *    - SHA-256 hashing            (utils/sha256_c.c)
 *    - Process fork/exec          (utils/process_c.c)
 *    - Error reporting + collector (error/error_c.c)
 *    - Color + path helpers       (c/color.c, c/kinetic_c.c)
 *
 *  The C++ layer (engine, DAG, phases, config) calls into the C layer
 *  exclusively through the prototypes declared here.
 * ============================================================================
 */
#ifndef KINETIC_H
#define KINETIC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────────────────
 *  Memo management — every C-side function that returns a freshly-allocated
 *  string takes a `kinetic_memo_t*` parameter.  Callers allocate the memo
 *  once (typically on the stack) and pass it down.  Strings are appended
 *  into a single owning arena; no individual frees are needed.
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    char   *base;     /* heap-allocated buffer  */
    size_t  cap;      /* capacity               */
    size_t  len;      /* bytes used             */
} kinetic_memo_t;

void kinetic_memo_init (kinetic_memo_t *m);
void kinetic_memo_reset(kinetic_memo_t *m);
void kinetic_memo_free (kinetic_memo_t *m);

/* Append N bytes (no NUL terminator implied) and return a pointer to a
 * private NUL-terminated copy.  The returned pointer is invalidated by the
 * next call into the same memo. */
const char *kinetic_memo_put(kinetic_memo_t *m, const char *data, size_t n);

/* Append a C string.  Convenience wrapper over kinetic_memo_put. */
const char *kinetic_memo_puts(kinetic_memo_t *m, const char *s);

/* ─────────────────────────────────────────────────────────────────────────────
 *  Color helpers — same palette as the previous C++ kinetic.hpp, exposed to
 *  C for log/error spans.
 * ────────────────────────────────────────────────────────────────────────── */
extern bool kinetic_color_enabled;

const char *kinetic_color(const char *ansi, const char *text);

#define KIN_ANSI_RESET   "\033[0m"
#define KIN_ANSI_BOLD    "\033[1m"
#define KIN_ANSI_DIM     "\033[2m"
#define KIN_ANSI_RED     "\033[31m"
#define KIN_ANSI_GREEN   "\033[32m"
#define KIN_ANSI_YELLOW  "\033[33m"
#define KIN_ANSI_BLUE    "\033[34m"
#define KIN_ANSI_CYAN    "\033[36m"
#define KIN_ANSI_MAGENTA "\033[35m"
#define KIN_ANSI_WHITE   "\033[37m"
#define KIN_ANSI_BRED    "\033[1;31m"
#define KIN_ANSI_BGREEN  "\033[1;32m"
#define KIN_ANSI_BYELLOW "\033[1;33m"
#define KIN_ANSI_BCYAN   "\033[1;36m"

/* ─────────────────────────────────────────────────────────────────────────────
 *  SHA-256 (FIPS-180-4) — same algorithm as the previous C++ implementation,
 *  rewritten in portable C99.
 * ────────────────────────────────────────────────────────────────────────── */

/* Hash `data` of length `len` and write the lowercase hex digest into `out`,
 * which must hold at least 65 bytes (64 hex chars + NUL). */
void kinetic_sha256_data(const uint8_t *data, size_t len, char out[65]);

/* Hash the contents of the file at `path`.  On success, writes the digest to
 * `out` (65 bytes) and returns 0.  On I/O error, returns -1 and leaves
 * `out` unmodified. */
int kinetic_sha256_file(const char *path, char out[65]);

/* ─────────────────────────────────────────────────────────────────────────────
 *  Process execution — fork/execvp with stdout+stderr capture, identical
 *  semantics to the previous C++ exec_proc + which + expand_home.
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int    exit_code;   /* process exit status, or -1 on internal failure */
    char  *out;        /* malloc'd, NUL-terminated stdout  */
    char  *err;        /* malloc'd, NUL-terminated stderr  */
    size_t out_len;
    size_t err_len;
} kinetic_proc_result_t;

int kinetic_exec_proc(const char *const *argv,    /* NULL-terminated */
                      const char       *cwd,       /* may be NULL     */
                      kinetic_proc_result_t *out);

void kinetic_proc_free(kinetic_proc_result_t *r);

/* PATH search for an executable.  Returns malloc'd absolute path (caller
 * frees), or NULL if not found.  If `name` already contains '/', returns
 * a copy of `name` when X_OK, otherwise NULL. */
char *kinetic_which(const char *name);

/* Expand a leading "~/" using $HOME.  Returns malloc'd string (caller frees),
 * or a copy of the input unchanged when no tilde expansion applies. */
char *kinetic_expand_home(const char *path);

/* ─────────────────────────────────────────────────────────────────────────────
 *  Error reporting + collector.
 *
 *  C++ uses this via the wrapper façade `kinetic::ErrorReporter`.  Phases
 *  collect errors during a run and `report()` after.
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    char *phase;       /* malloc'd  */
    char *code;        /* malloc'd  */
    char *short_desc;  /* malloc'd  */
    char *file;        /* malloc'd, may be NULL */
    int   line;
    int   col;
    char *detail;      /* malloc'd, may be NULL */
    char *hint;        /* malloc'd, may be NULL */
} kinetic_error_t;

typedef struct {
    kinetic_error_t *items;   /* malloc'd array */
    size_t           count;
    size_t           cap;
} kinetic_error_collector_t;

void kinetic_ec_init (kinetic_error_collector_t *ec);
void kinetic_ec_free (kinetic_error_collector_t *ec);

/* Append a new error; ownership of `file`/`detail`/`hint` is taken by the
 * collector (caller must *not* free them when this returns 0).
 *
 * `file`, `detail`, `hint` may be NULL or "" — they are copied internally
 * (so passing static strings is safe).  Returns 0 on success, -1 on OOM. */
int kinetic_ec_add(kinetic_error_collector_t *ec,
                   const char *phase,
                   const char *code,
                   const char *short_desc,
                   const char *file,
                   int         line,
                   int         col,
                   const char *detail,
                   const char *hint);

/* Pretty-print a single error block (ANSI) to stderr, in the same shape as
 * the previous C++ report_error().  Honors kinetic_color_enabled. */
void kinetic_error_print(const kinetic_error_t *e);

/* Print all collected errors, then clear the collector. */
void kinetic_error_print_all(kinetic_error_collector_t *ec);

/* Print a phase-prefixed info/warning line to stdout. */
void kinetic_phase_log (const char *phase, const char *msg);
void kinetic_phase_warn(const char *phase, const char *msg);

/* Print a phase-prefixed error block to stderr and THROW-like semantics:
 * in C just prints; in C++ the wrapper at kinetic::fatal will throw after
 * calling this. */
void kinetic_phase_fatal(const char *phase,
                          const char *code,
                          const char *short_desc,
                          const char *file,
                          const char *detail);

/* Lookup the hint text for an error code.  Returns NULL if no hint is
 * registered.  The returned pointer lives for the program's duration. */
const char *kinetic_hint_for_code(const char *code);

/* ─────────────────────────────────────────────────────────────────────────────
 *  C++ entry — declared here so main.c can invoke the engine without
 *  pulling in std::filesystem, std::variant, etc.
 * ────────────────────────────────────────────────────────────────────────── */

/* Parse CLI args, drive the engine, return process exit code.  `cwd` is the
 * project root (typically CWD).  declared-noexcept at the C++ side via
 * try/catch — never throws into C. */
int kinetic_engine_run(int argc, char **argv, const char *cwd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KINETIC_H */
