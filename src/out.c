#include "out.h"

void out_init(struct out *o, int kind, FILE *fp)
{
    o->kind = kind;
    o->ascii = 0;
    o->mono = 0;
    o->color = -1;
    o->fp = fp;
    o->page = 0;
    o->bios = 0;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

int mk_width(const char *s)
{
    int n = 0;

    while (*s != '\0') {
        if (*s == MK) {
            if (*++s != '\0')
                s++;
        } else {
            s++;
            n++;
        }
    }
    return n;
}

int cp437_ascii(int c)
{
    c &= 0xff;
    if (c < 0x80)
        return c;
    switch (c) {
    case 0xb0: case 0xb1:               /* light and medium shade */
        return '.';
    case 0xb2: case 0xdb:               /* dark shade, full block */
        return '#';
    case 0xdc:                          /* lower half block */
        return '_';
    case 0xdf:                          /* upper half block */
        return '"';
    case 0xdd: case 0xde:               /* left and right half */
        return '#';
    case 0xb3: case 0xba:               /* vertical lines */
        return '|';
    case 0xc4: case 0xcd:               /* horizontal lines */
        return '-';
    case 0xfa: case 0xf9:               /* middle dots */
        return '.';
    case 0xfe:                          /* small square */
        return '#';
    }
    if (c >= 0xb4 && c <= 0xda)         /* box corners and tees */
        return '+';
    return '?';
}

unsigned out_attr(const struct out *o, int color)
{
    if (!o->mono)
        return (unsigned)color;
    /* dark gray is a dim color, so normal */
    return color >= 9 ? 0x0f : 0x07;
}

/* ANSI colors are in RGB bit order, CGA colors in BGR. */
static const char ansi_of[8] = { '0', '4', '2', '6', '1', '5', '3', '7' };

static void set_color(struct out *o, int c)
{
    if (c == o->color)
        return;
    o->color = c;
    if (o->kind == OUT_ANSI) {
        char esc[8];

        esc[0] = '\x1b';
        esc[1] = '[';
        esc[2] = c >= 8 ? '1' : '0';
        esc[3] = ';';
        esc[4] = '3';
        esc[5] = ansi_of[c & 7];
        esc[6] = 'm';
        esc[7] = '\0';
        fputs(esc, o->fp);
    }
}

static void put_char(struct out *o, int c)
{
    if (o->ascii)
        c = cp437_ascii(c);
    if (o->kind == OUT_BIOS) {
        o->bios(o, c & 0xff,
                out_attr(o, o->color < 0 ? COLOR_DEFAULT : o->color));
        return;
    }
    putc(c, o->fp);
}

int out_text(struct out *o, const char *s, int max)
{
    int n = 0;

    while (*s != '\0') {
        if (*s == MK) {
            int c = hexval(s[1]);

            if (s[1] == '\0')
                break;
            if (c >= 0)
                set_color(o, c);
            s += 2;
            continue;
        }
        if (max >= 0 && n >= max)
            break;
        put_char(o, (unsigned char)*s++);
        n++;
    }
    return n;
}

void out_spaces(struct out *o, int n)
{
    while (n-- > 0)
        put_char(o, ' ');
}

void out_nl(struct out *o)
{
    if (o->kind == OUT_ANSI && o->color >= 0)
        fputs("\x1b[0m", o->fp);
    o->color = -1;
    if (o->kind == OUT_BIOS)
        o->bios(o, -1, 0);
    else
        putc('\n', o->fp);
}
