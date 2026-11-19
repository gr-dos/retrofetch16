#ifndef OUT_H
#define OUT_H

#include <stdio.h>

/*
 * Colored text output.  Portable: no DOS calls (the BIOS backend is in
 * video.c).
 *
 * Text is written in a small markup: the byte MK (01h) followed by one
 * hex digit sets the foreground to that CGA color (0-9, a-f).  Every
 * other byte is shown as it is, in code page 437.  Line breaks go
 * through out_nl, never as "\n" in the text.
 */

#define MK      '\x01'

/* CGA colors, as markup strings */
#define C_BLACK     "\x01" "0"
#define C_BLUE      "\x01" "1"
#define C_GREEN     "\x01" "2"
#define C_CYAN      "\x01" "3"
#define C_RED       "\x01" "4"
#define C_MAGENTA   "\x01" "5"
#define C_BROWN     "\x01" "6"
#define C_GRAY      "\x01" "7"
#define C_DGRAY     "\x01" "8"
#define C_LBLUE     "\x01" "9"
#define C_LGREEN    "\x01" "a"
#define C_LCYAN     "\x01" "b"
#define C_LRED      "\x01" "c"
#define C_LMAGENTA  "\x01" "d"
#define C_YELLOW    "\x01" "e"
#define C_WHITE     "\x01" "f"

#define COLOR_DEFAULT 7

/* Backends */
#define OUT_PLAIN   0       /* no color: standard output */
#define OUT_ANSI    1       /* ANSI SGR escapes: standard output */
#define OUT_BIOS    2       /* INT 10h attributes: the screen */

struct out {
    int kind;               /* OUT_* */
    int ascii;              /* map code page 437 graphics to ASCII */
    int mono;               /* monochrome attributes (bright or normal) */
    int color;              /* current color, -1 for none yet */
    FILE *fp;               /* OUT_PLAIN, OUT_ANSI */
    unsigned page;          /* OUT_BIOS: the active display page */
    /* OUT_BIOS: writes one character with an attribute and advances
     * the cursor, or (c < 0) a line break.  Set by video.c. */
    void (*bios)(struct out *o, int c, unsigned attr);
};

void out_init(struct out *o, int kind, FILE *fp);

/* Writes markup, at most max visible characters of it (max < 0: all).
 * Returns the number of visible characters written. */
int out_text(struct out *o, const char *s, int max);

/* n spaces */
void out_spaces(struct out *o, int n);

/* End of line: resets the color.  CR LF on DOS. */
void out_nl(struct out *o);

/* Visible width of markup. */
int mk_width(const char *s);

/* The ASCII stand-in for a code page 437 character (itself if ASCII). */
int cp437_ascii(int c);

/* The text attribute for a color: itself in color; in monochrome 0Fh
 * (bright) for the bright colors 9 to 15, 07h for the rest. */
unsigned out_attr(const struct out *o, int color);

#endif
