/* Host unit tests for src/options.c */

#include <string.h>
#include "logo.h"
#include "options.h"
#include "unit.h"

static FILE *err;

static int parse(struct options *o, const char *a1, const char *a2)
{
    char *argv[4];
    int argc = 1;

    argv[0] = (char *)"retrofch";
    if (a1 != NULL)
        argv[argc++] = (char *)a1;
    if (a2 != NULL)
        argv[argc++] = (char *)a2;
    argv[argc] = NULL;
    return parse_options(argc, argv, o, err);
}

int main(void)
{
    struct options o;

    err = tmpfile();

    CHECK(parse(&o, NULL, NULL) == OPT_RUN);
    CHECK(o.mode == MODE_AUTO && !o.ascii && !o.all && o.logo == -1);

    CHECK(parse(&o, "--help", NULL) == OPT_HELP);
    CHECK(parse(&o, "-h", NULL) == OPT_HELP);
    CHECK(parse(&o, "-H", NULL) == OPT_HELP);
    CHECK(parse(&o, "-?", NULL) == OPT_HELP);
    CHECK(parse(&o, "/?", NULL) == OPT_HELP);
    CHECK(parse(&o, "--HELP", NULL) == OPT_HELP);
    CHECK(parse(&o, "--version", NULL) == OPT_VERSION);
    CHECK(parse(&o, "--Version", NULL) == OPT_VERSION);
    /* the first of help or version wins, before any error */
    CHECK(parse(&o, "--version", "--bogus") == OPT_VERSION);

    CHECK(parse(&o, "-a", "--ascii") == OPT_RUN);
    CHECK(o.all && o.ascii);
    CHECK(parse(&o, "--ALL", NULL) == OPT_RUN && o.all);
    CHECK(parse(&o, "--plain", NULL) == OPT_RUN && o.mode == MODE_PLAIN);
    CHECK(parse(&o, "--ansi", NULL) == OPT_RUN && o.mode == MODE_ANSI);
    CHECK(parse(&o, "--mono", NULL) == OPT_RUN && o.mode == MODE_MONO);
    /* the last mode wins */
    CHECK(parse(&o, "--ansi", "--plain") == OPT_RUN && o.mode == MODE_PLAIN);

    CHECK(parse(&o, "--logo=grdos", NULL) == OPT_RUN);
    CHECK(o.logo == LOGO_GRDOS);
    CHECK(parse(&o, "--LOGO=DOS", NULL) == OPT_RUN && o.logo == LOGO_DOS);
    CHECK(parse(&o, "--logo=none", NULL) == OPT_RUN && o.logo == LOGO_NONE);

    CHECK(parse(&o, "--logo=bogus", NULL) == OPT_USAGE);
    CHECK(parse(&o, "--logo=", NULL) == OPT_USAGE);
    CHECK(parse(&o, "--logo", NULL) == OPT_USAGE);
    CHECK(parse(&o, "--bogus", NULL) == OPT_USAGE);
    CHECK(parse(&o, "-x", NULL) == OPT_USAGE);
    CHECK(parse(&o, "/x", NULL) == OPT_USAGE);
    CHECK(parse(&o, "-", NULL) == OPT_USAGE);
    CHECK(parse(&o, "extra", NULL) == OPT_USAGE);

    /* the diagnostic */
    {
        char buf[256];
        size_t n;

        fclose(err);
        err = tmpfile();
        parse(&o, "--bogus", NULL);
        rewind(err);
        n = fread(buf, 1, sizeof buf - 1, err);
        buf[n] = '\0';
        CHECK(strcmp(buf, "retrofch: unrecognized option '--bogus'\n"
                     "Try 'retrofch --help' for more information.\n") == 0);
    }
    fclose(err);
    return UNIT_DONE();
}
