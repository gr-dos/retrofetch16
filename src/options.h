#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdio.h>

/* Command-line parsing.  Portable. */

#define OPT_RUN     0
#define OPT_HELP    1
#define OPT_VERSION 2
#define OPT_USAGE   3       /* a diagnostic has been written */

#define MODE_AUTO   0       /* BIOS colors on the screen, else plain */
#define MODE_PLAIN  1
#define MODE_ANSI   2
#define MODE_MONO   3

struct options {
    int mode;               /* MODE_* */
    int ascii;              /* 7-bit ASCII art */
    int all;                /* query A:, B:, removable and CD-ROM drives */
    int logo;               /* LOGO_*, or -1 to choose by the kernel */
};

int parse_options(int argc, char **argv, struct options *o, FILE *err);

#endif
