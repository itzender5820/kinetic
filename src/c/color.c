/*
 * ============================================================================
 *  c/color.c  —  Tiny ANSI-color front-end used by the C layer.
 * ============================================================================
 */
#include "kinetic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool kinetic_color_enabled = true;

const char *kinetic_color(const char *ansi, const char *text)
{
    if (!kinetic_color_enabled || ansi == NULL || text == NULL) return text;

    size_t ansi_len = strlen(ansi);
    size_t txt_len  = strlen(text);
    size_t total    = ansi_len + txt_len + strlen(KIN_ANSI_RESET) + 1;

    char *buf = (char *)malloc(total);
    if (buf == NULL) return text;
    memcpy(buf, ansi, ansi_len);
    memcpy(buf + ansi_len, text, txt_len);
    strcpy(buf + ansi_len + txt_len, KIN_ANSI_RESET);
    return buf;
}
