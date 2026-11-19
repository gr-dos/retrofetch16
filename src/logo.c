/*
 * Original artwork for retrofetch16, drawn in code page 437 blocks and
 * box lines.  Under the repository's license (LICENSE); no third-party
 * artwork or fonts.  Each line is exactly the logo's width.
 */

#include <stddef.h>
#include "logo.h"
#include "out.h"

#define FB  "\xdb"          /* full block */
#define UB  "\xdf"          /* upper half block */
#define LB  "\xdc"          /* lower half block */
#define HL  "\xc4"          /* horizontal line */
#define VL  "\xb3"          /* vertical line */
#define TL  "\xda"          /* corners */
#define TR  "\xbf"
#define BL  "\xc0"
#define BR  "\xd9"
#define DOT "\xfa"          /* middle dot */

/* "GR" in two colors over "DOS", 19 columns. */
static const char *const grdos_lines[] = {
    C_LCYAN " " LB FB FB FB FB LB "   " C_YELLOW FB FB FB FB FB FB LB "  ",
    C_LCYAN FB FB UB "  " UB UB "   " C_YELLOW FB FB "   " FB FB "  ",
    C_LCYAN FB FB "  " LB LB LB "   " C_YELLOW FB FB FB FB FB FB UB "  ",
    C_LCYAN FB FB "   " FB FB "   " C_YELLOW FB FB "  " UB FB LB "  ",
    C_LCYAN " " UB FB FB FB FB UB "   " C_YELLOW FB FB "    " FB FB " ",
    "                   ",
    C_DGRAY HL HL HL HL HL HL " " C_WHITE "D" C_DGRAY DOT C_WHITE "O"
        C_DGRAY DOT C_WHITE "S" C_DGRAY " " HL HL HL HL HL HL,
    NULL
};

/* A monitor at a DOS prompt, 20 columns. */
static const char *const dos_lines[] = {
    C_GRAY TL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL TR,
    C_GRAY VL C_LGREEN " C:\\>ver          " C_GRAY VL,
    C_GRAY VL C_LGREEN " DOS              " C_GRAY VL,
    C_GRAY VL C_LGREEN " C:\\>" C_WHITE "_" C_LGREEN "            " C_GRAY VL,
    C_GRAY BL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL HL BR,
    C_DGRAY "      " LB LB LB LB LB LB LB LB "      ",
    C_DGRAY "   " UB UB UB UB UB UB UB UB UB UB UB UB UB UB "   ",
    NULL
};

static const char *const no_lines[] = { NULL };

static const struct logo logos[] = {
    { "none",  0,  15, no_lines },
    { "dos",   20, 10, dos_lines },
    { "grdos", 19, 11, grdos_lines },
};

const struct logo *logo_get(int which)
{
    if (which < 0 || which > LOGO_GRDOS)
        which = LOGO_NONE;
    return &logos[which];
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int logo_byname(const char *name)
{
    int i;

    for (i = 0; i <= LOGO_GRDOS; i++) {
        const char *a = name, *b = logos[i].name;

        while (*a != '\0' && lower(*a) == *b) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0')
            return i;
    }
    return -1;
}
