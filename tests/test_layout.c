/* Host unit tests for src/out.c, src/logo.c and src/layout.c */

#include <stdlib.h>
#include <string.h>
#include "layout.h"
#include "logo.h"
#include "out.h"
#include "unit.h"

/* Renders into a temporary file and returns its contents. */
static char got[8192];

static const char *capture(int kind, int ascii, const struct logo *lg,
                           const struct report *r, int palette, int width)
{
    FILE *fp = tmpfile();
    struct out o;
    size_t n;

    out_init(&o, kind, fp);
    o.ascii = ascii;
    render(&o, lg, r, palette, width);
    rewind(fp);
    n = fread(got, 1, sizeof got - 1, fp);
    got[n] = '\0';
    fclose(fp);
    return got;
}

/* The longest line of s, in bytes. */
static int widest(const char *s)
{
    int w = 0, n = 0;

    for (; *s != '\0'; s++) {
        if (*s == '\n') {
            if (n > w)
                w = n;
            n = 0;
        } else {
            n++;
        }
    }
    return n > w ? n : w;
}

static int count(const char *s, int c)
{
    int n = 0;

    for (; *s != '\0'; s++)
        n += *s == c;
    return n;
}

static int str_is(const char *got_, const char *want)
{
    if (strcmp(got_, want) != 0) {
        printf("  got:\n%s\n  want:\n%s\n", got_, want);
        return 0;
    }
    return 1;
}

int main(void)
{
    struct report r;
    const struct logo *lg;
    const char *s;
    int i, k;

    /* every logo line is exactly the logo's width */
    for (k = LOGO_NONE; k <= LOGO_GRDOS; k++) {
        lg = logo_get(k);
        for (i = 0; lg->lines[i] != NULL; i++)
            if (mk_width(lg->lines[i]) != lg->width) {
                printf("  logo %s line %d: width %d, want %d\n", lg->name,
                       i, mk_width(lg->lines[i]), lg->width);
                CHECK(0);
            }
        CHECK(lg->width + LOGO_GAP + 40 < 80);
    }
    CHECK(logo_get(-5) == logo_get(LOGO_NONE));
    CHECK(logo_byname("GRDOS") == LOGO_GRDOS);
    CHECK(logo_byname("dos") == LOGO_DOS);
    CHECK(logo_byname("None") == LOGO_NONE);
    CHECK(logo_byname("fdos") == -1);
    CHECK(logo_byname("") == -1);

    /* markup */
    CHECK(mk_width("") == 0);
    CHECK(mk_width(C_RED "ab" C_WHITE "c") == 3);
    CHECK(mk_width("ab\x01") == 2);     /* a dangling marker is ignored */
    CHECK(cp437_ascii('A') == 'A');
    CHECK(cp437_ascii(0xdb) == '#');
    CHECK(cp437_ascii(0xc4) == '-');
    CHECK(cp437_ascii(0xb3) == '|');
    CHECK(cp437_ascii(0xda) == '+');
    CHECK(cp437_ascii(0x80) == '?');

    /* attributes */
    {
        struct out o;

        out_init(&o, OUT_BIOS, NULL);
        CHECK(out_attr(&o, 10) == 10);
        CHECK(out_attr(&o, 0) == 0);
        o.mono = 1;
        CHECK(out_attr(&o, 15) == 0x0f);
        CHECK(out_attr(&o, 9) == 0x0f);
        CHECK(out_attr(&o, 8) == 0x07);     /* dark gray stays dim */
        CHECK(out_attr(&o, 1) == 0x07);     /* not underline on an MDA */
        CHECK(out_attr(&o, 0) == 0x07);     /* never invisible */
    }

    /* a two-line report beside no logo: plain text, colors dropped */
    r.n = 2;
    strcpy(r.line[0], C_LCYAN "host");
    strcpy(r.line[1], C_LCYAN "OS: " C_GRAY "DOS 5.00");
    s = capture(OUT_PLAIN, 0, logo_get(LOGO_NONE), &r, 0, 80);
    CHECK(str_is(s, "host\nOS: DOS 5.00\n\n"));

    /* ANSI: SGR codes, reset at the end of each colored line */
    s = capture(OUT_ANSI, 0, logo_get(LOGO_NONE), &r, 0, 80);
    CHECK(str_is(s, "\x1b[1;36mhost\x1b[0m\n"
                    "\x1b[1;36mOS: \x1b[0;37mDOS 5.00\x1b[0m\n\n"));

    /* cut to width - 1 */
    s = capture(OUT_PLAIN, 0, logo_get(LOGO_NONE), &r, 0, 8);
    CHECK(str_is(s, "host\nOS: DOS\n\n"));

    /* beside the DOS logo: the report starts after the logo and the
     * gap; rows past the report carry the logo alone, without trailing
     * spaces */
    lg = logo_get(LOGO_DOS);
    s = capture(OUT_PLAIN, 1, lg, &r, 0, 80);
    CHECK(count(s, '\n') == 8);         /* 7 logo rows, then a blank */
    CHECK(strncmp(s, "+------------------+   host\n", 28) == 0);
    CHECK(strstr(s, "| C:\\>ver          |   OS: DOS 5.00\n") != NULL);
    CHECK(strstr(s, "| DOS              |\n") != NULL);
    CHECK(strstr(s, "   \"\"\"\"\"\"\"\"\"\"\"\"\"\"\n") != NULL);
    CHECK(strchr(s, '\x01') == NULL);
    for (i = 0; s[i] != '\0'; i++)
        if ((unsigned char)s[i] >= 0x80)
            break;
    CHECK(s[i] == '\0');                /* --ascii: 7-bit only */

    /* a report longer than the logo, and the palette */
    r.n = 10;
    for (i = 2; i < r.n; i++)
        strcpy(r.line[i], "x");
    s = capture(OUT_ANSI, 0, lg, &r, 1, 80);
    CHECK(count(s, '\n') == 10 + 3 + 1);
    CHECK(strstr(s, "\x1b[0;30m\xdb\xdb\xdb\x1b[0;34m\xdb\xdb\xdb") != NULL);
    CHECK(strstr(s, "\x1b[1;37m\xdb\xdb\xdb\x1b[0m\n") != NULL);
    /* rows past the logo are indented by its width and the gap */
    CHECK(strstr(s, "\n                       x") != NULL);

    /* nothing is wider than 79 columns, however long the report */
    for (i = 0; i < r.n; i++)
        memset(r.line[i], 'y', REPORT_SIZE - 1), r.line[i][REPORT_SIZE - 1] = 0;
    s = capture(OUT_PLAIN, 0, logo_get(LOGO_GRDOS), &r, 0, 80);
    CHECK(widest(s) == 79);
    s = capture(OUT_PLAIN, 0, logo_get(LOGO_NONE), &r, 0, 40);
    CHECK(widest(s) == 39);

    return UNIT_DONE();
}
