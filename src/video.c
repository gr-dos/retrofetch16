/*
 * The BIOS output backend: color without ANSI.SYS.  DOS only.
 *
 * Each character is written with its attribute by INT 10h AH=09h, then
 * again by the teletype call AH=0Eh, which moves the cursor (and
 * scrolls) but keeps the attribute already at that cell.
 */

#include <dos.h>
#include "video.h"

static void bios_put(struct out *o, int c, unsigned attr)
{
    union REGS r;

    if (c < 0) {
        r.x.ax = 0x0e0d;
        r.h.bh = (unsigned char)o->page;
        int86(0x10, &r, &r);
        r.x.ax = 0x0e0a;
        r.h.bh = (unsigned char)o->page;
        int86(0x10, &r, &r);
        return;
    }
    r.h.ah = 0x09;
    r.h.al = (unsigned char)c;
    r.h.bh = (unsigned char)o->page;
    r.h.bl = (unsigned char)attr;
    r.x.cx = 1;
    int86(0x10, &r, &r);
    r.h.ah = 0x0e;
    r.h.al = (unsigned char)c;
    r.h.bh = (unsigned char)o->page;
    int86(0x10, &r, &r);
}

void video_out(struct out *o)
{
    union REGS r;

    out_init(o, OUT_BIOS, 0);
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    o->page = r.h.bh;
    o->bios = bios_put;
}
