#include <string.h>
#include "logo.h"
#include "options.h"

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Case-insensitive equality with a lowercase literal. */
static int same(const char *a, const char *lit)
{
    while (*a != '\0' && lower((unsigned char)*a) == *lit) {
        a++;
        lit++;
    }
    return *a == '\0' && *lit == '\0';
}

/* If a starts with the lowercase literal prefix (any case), the rest. */
static const char *after(const char *a, const char *prefix)
{
    while (*prefix != '\0') {
        if (lower((unsigned char)*a) != *prefix)
            return NULL;
        a++;
        prefix++;
    }
    return a;
}

static int bad(FILE *err, const char *what, const char *arg)
{
    fputs("retrofch: ", err);
    fputs(what, err);
    fputs(" '", err);
    fputs(arg, err);
    fputs("'\n", err);
    fputs("Try 'retrofch --help' for more information.\n", err);
    return OPT_USAGE;
}

int parse_options(int argc, char **argv, struct options *o, FILE *err)
{
    int i;

    o->mode = MODE_AUTO;
    o->ascii = 0;
    o->all = 0;
    o->logo = -1;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v;

        if (same(a, "--help") || same(a, "-h") || same(a, "-?") ||
            same(a, "/?"))
            return OPT_HELP;
        if (same(a, "--version"))
            return OPT_VERSION;
        if (same(a, "-a") || same(a, "--all"))
            o->all = 1;
        else if (same(a, "--plain"))
            o->mode = MODE_PLAIN;
        else if (same(a, "--ansi"))
            o->mode = MODE_ANSI;
        else if (same(a, "--mono"))
            o->mode = MODE_MONO;
        else if (same(a, "--ascii"))
            o->ascii = 1;
        else if ((v = after(a, "--logo=")) != NULL) {
            if ((o->logo = logo_byname(v)) < 0)
                return bad(err, "unknown logo", v);
        } else if (a[0] == '-' || a[0] == '/')
            return bad(err, "unrecognized option", a);
        else
            return bad(err, "extra operand", a);
    }
    return OPT_RUN;
}
