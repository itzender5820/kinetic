/*
 * ============================================================================
 *  c/kinetic_c.c  —  Memo arena + tiny C-string helpers
 * ============================================================================
 */
#include "kinetic.h"
#include <stdlib.h>
#include <string.h>

#define KINETIC_MEMO_INITIAL_CAP 4096

void kinetic_memo_init(kinetic_memo_t *m)
{
    m->base = (char *)malloc(KINETIC_MEMO_INITIAL_CAP);
    m->cap  = m->base ? KINETIC_MEMO_INITIAL_CAP : 0;
    m->len  = 0;
    if (m->base) m->base[0] = '\0';
}

void kinetic_memo_reset(kinetic_memo_t *m)
{
    if (m->base == NULL) {
        kinetic_memo_init(m);
        return;
    }
    m->len = 0;
    m->base[0] = '\0';
}

void kinetic_memo_free(kinetic_memo_t *m)
{
    free(m->base);
    m->base = NULL;
    m->cap  = 0;
    m->len  = 0;
}

const char *kinetic_memo_put(kinetic_memo_t *m, const char *data, size_t n)
{
    if (m == NULL || (n > 0 && data == NULL)) return NULL;
    if (m->base == NULL) kinetic_memo_init(m);
    if (m->base == NULL) return NULL;

    /* need n + 1 (NUL) */
    if (m->len + n + 1 > m->cap) {
        size_t need  = m->len + n + 1;
        size_t ncap  = m->cap ? m->cap : KINETIC_MEMO_INITIAL_CAP;
        while (ncap < need) {
            if (ncap > (SIZE_MAX / 2)) { ncap = need; break; }
            ncap *= 2;
        }
        char *nb = (char *)realloc(m->base, ncap);
        if (nb == NULL) return NULL;
        m->base = nb;
        m->cap  = ncap;
    }

    const size_t off = m->len;
    if (n) memcpy(m->base + off, data, n);
    m->base[off + n] = '\0';
    m->len = off + n;
    return m->base + off;
}

const char *kinetic_memo_puts(kinetic_memo_t *m, const char *s)
{
    if (s == NULL) return NULL;
    return kinetic_memo_put(m, s, strlen(s));
}
