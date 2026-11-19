#include <stddef.h>
#include "layout.h"

/* Visible width of markup without its trailing spaces. */
static int trimmed_width(const char *s)
{
    int n = 0, last = 0;

    while (*s != '\0') {
        if (*s == MK) {
            if (*++s != '\0')
                s++;
            continue;
        }
        n++;
        if (*s != ' ')
            last = n;
        s++;
    }
    return last;
}

static const char *const swatch[16] = {
    C_BLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_BROWN, C_GRAY,
    C_DGRAY, C_LBLUE, C_LGREEN, C_LCYAN, C_LRED, C_LMAGENTA, C_YELLOW,
    C_WHITE
};

void render(struct out *o, const struct logo *lg, const struct report *r,
            int palette, int width)
{
    int nlogo = 0, ntext = r->n, rows, i, room;
    int left = lg->width > 0 ? lg->width + LOGO_GAP : 0;

    while (lg->lines[nlogo] != NULL)
        nlogo++;
    if (palette)
        ntext += 3;
    rows = nlogo > ntext ? nlogo : ntext;
    room = width - 1 - left;
    if (room < 0)
        room = 0;

    for (i = 0; i < rows; i++) {
        const char *text = NULL;
        int pal = -1;

        if (i < r->n)
            text = r->line[i];
        else if (palette && i >= r->n + 1 && i < r->n + 3)
            pal = (i - r->n - 1) * 8;

        if (text != NULL || pal >= 0) {
            if (i < nlogo)
                out_text(o, lg->lines[i], lg->width);
            else
                out_spaces(o, lg->width);
            out_spaces(o, left - lg->width);
        } else if (i < nlogo) {
            out_text(o, lg->lines[i], trimmed_width(lg->lines[i]));
        }

        if (text != NULL) {
            out_text(o, text, room);
        } else if (pal >= 0) {
            int k, n = 0;

            for (k = 0; k < 8 && n + 3 <= room; k++, n += 3) {
                out_text(o, swatch[pal + k], 0);
                out_text(o, "\xdb\xdb\xdb", 3);
            }
        }
        out_nl(o);
    }
    out_nl(o);
}
