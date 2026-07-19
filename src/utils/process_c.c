/*
 * ============================================================================
 *  utils/process_c.c  —  Pure-C subprocess execution with stdout+stderr
 *  capture.  Identical semantics to the C++ exec_proc(); rewritten as
 *  fork/execvp + poll() without any C++ dependency.
 * ============================================================================
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "kinetic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>

char *kinetic_which(const char *name)
{
    if (name == NULL) return NULL;

    /* If name already contains a path separator, validate X_OK on it. */
    if (strchr(name, '/') != NULL) {
        if (access(name, X_OK) == 0) {
            char *dup = strdup(name);
            return dup;
        }
        return NULL;
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL || path_env[0] == '\0') return NULL;

    const char *p = path_env;
    while (*p != '\0') {
        const char *colon = strchr(p, ':');
        size_t      dlen  = colon ? (size_t)(colon - p) : strlen(p);

        /* Compose candidate path: dir + "/" + name + NUL */
        size_t      need  = dlen + 1 + strlen(name) + 1;
        char       *full  = (char *)malloc(need);
        if (full == NULL) return NULL;
        memcpy(full, p, dlen);
        full[dlen] = '/';
        strcpy(full + dlen + 1, name);

        if (access(full, X_OK) == 0) return full;
        free(full);

        if (colon == NULL) break;
        p = colon + 1;
    }
    return NULL;
}

char *kinetic_expand_home(const char *path)
{
    if (path == NULL) return NULL;
    if (path[0] != '~') {
        char *dup = strdup(path);
        return dup;
    }
    /* path[0] == '~' */
    const char *home = getenv("HOME");
    if (home == NULL) {
        char *dup = strdup(path);
        return dup;
    }
    size_t hlen = strlen(home);
    size_t plen = strlen(path);   /* includes the leading '~' */

    char *out = (char *)malloc(hlen + plen);  /* plen-1 + NUL */
    if (out == NULL) return NULL;
    memcpy(out, home, hlen);
    /* skip '~' in src */
    if (plen > 1) memcpy(out + hlen, path + 1, plen - 1);
    out[hlen + plen - 1] = '\0';
    return out;
}

/* ──────────────────────────────────────────────────────────────────────
 *  execvp-based subprocess with two captured pipes + poll().
 * ────────────────────────────────────────────────────────────────────── */
int kinetic_exec_proc(const char *const *argv,
                      const char       *cwd,
                      kinetic_proc_result_t *result)
{
    if (result == NULL) return -1;
    result->exit_code = -1;
    result->out       = NULL;  result->out_len = 0;
    result->err       = NULL;  result->err_len = 0;

    if (argv == NULL || argv[0] == NULL) return -1;

    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        if (out_pipe[0] != -1) close(out_pipe[0]);
        if (out_pipe[1] != -1) close(out_pipe[1]);
        if (err_pipe[0] != -1) close(err_pipe[0]);
        if (err_pipe[1] != -1) close(err_pipe[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* ── Child ────────────────────────────────────────────────────── */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);

        if (cwd != NULL && cwd[0] != '\0') {
            if (chdir(cwd) != 0) _exit(127);
        }

        /* execvp needs a non-const char *argv[], but the contents are
         * not mutated — the cast is the usual POSIX-blessed pattern. */
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    /* ── Parent ───────────────────────────────────────────────────────── */
    close(out_pipe[1]);
    close(err_pipe[1]);

    /* Grow-able buffers for stdout/stderr */
    char  *out_buf = NULL;  size_t out_cap = 0, out_len = 0;
    char  *err_buf = NULL;  size_t err_cap = 0, err_len = 0;

    struct pollfd fds[2];
    fds[0].fd = out_pipe[0]; fds[0].events = POLLIN;
    fds[1].fd = err_pipe[0]; fds[1].events = POLLIN;

    char buf[4096];
    int  open_count = 2;

    while (open_count > 0) {
        int ready = poll(fds, 2, 5000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0) continue;
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                if (n > 0) {
                    char **dst   = (i == 0) ? &out_buf : &err_buf;
                    size_t *dlen = (i == 0) ? &out_len : &err_len;
                    size_t *dcap = (i == 0) ? &out_cap : &err_cap;

                    if (*dlen + (size_t)n + 1 > *dcap) {
                        size_t ncap = (*dcap == 0) ? 4096 : *dcap * 2;
                        while (ncap < *dlen + (size_t)n + 1) {
                            if (ncap > (SIZE_MAX / 2)) { ncap = *dlen + (size_t)n + 1; break; }
                            ncap *= 2;
                        }
                        char *nb = (char *)realloc(*dst, ncap);
                        if (nb != NULL) {
                            *dst  = nb;
                            *dcap = ncap;
                        } else {
                            /* Treat OOM as terminal — drop the read */
                        }
                    }
                    if (*dst != NULL) {
                        memcpy(*dst + *dlen, buf, (size_t)n);
                        *dlen += (size_t)n;
                        (*dst)[*dlen] = '\0';
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --open_count;
                }
            }
        }
    }

    /* Drain */
    for (int i = 0; i < 2; ++i) {
        if (fds[i].fd < 0) continue;
        for (;;) {
            ssize_t n = read(fds[i].fd, buf, sizeof(buf));
            if (n <= 0) break;
            char **dst   = (i == 0) ? &out_buf : &err_buf;
            size_t *dlen = (i == 0) ? &out_len : &err_len;
            size_t *dcap = (i == 0) ? &out_cap : &err_cap;
            if (*dlen + (size_t)n + 1 > *dcap) {
                size_t ncap = (*dcap == 0) ? 4096 : *dcap * 2;
                while (ncap < *dlen + (size_t)n + 1) ncap *= 2;
                char *nb = (char *)realloc(*dst, ncap);
                if (nb != NULL) { *dst = nb; *dcap = ncap; }
            }
            if (*dst != NULL) {
                memcpy(*dst + *dlen, buf, (size_t)n);
                *dlen += (size_t)n;
                (*dst)[*dlen] = '\0';
            }
        }
        close(fds[i].fd);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result->out       = out_buf;
    result->err       = err_buf;
    result->out_len   = out_len;
    result->err_len   = err_len;
    return 0;
}

void kinetic_proc_free(kinetic_proc_result_t *r)
{
    if (r == NULL) return;
    free(r->out);
    free(r->err);
    r->out = NULL; r->err = NULL;
    r->out_len = 0; r->err_len = 0;
}
