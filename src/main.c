/*
 * ============================================================================
 *  main.c  —  Kinetic C entry point.
 *
 *  All work is delegated to kinetic_engine_run() declared in kinetic.h.
 *  main() only deals with argv plumbing, deletes nothing, throws nothing.
 * ============================================================================
 */
#include "kinetic.h"
#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>

static int determine_cwd(char *buf, size_t cap)
{
    if (getcwd(buf, cap) == NULL) return -1;
    return 0;
}

int main(int argc, char **argv)
{
    char cwd[PATH_MAX];
    if (determine_cwd(cwd, sizeof(cwd)) != 0) {
        fprintf(stderr, "\033[1;31mFatal: \033[0mcannot determine current directory\n");
        return 1;
    }
    return kinetic_engine_run(argc, argv, cwd);
}
