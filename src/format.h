#ifndef FORMAT_H
#define FORMAT_H

/*
 * Capacity arithmetic and size formatting.  Portable: no DOS calls.
 *
 * Adapted from gr-dos/df16 (src/format.c, MIT): a DOS free-space query
 * gives counts whose product can exceed 32 bits (2^48 for INT 21h
 * AH=36h, more for AX=7303h), so byte counts are 64-bit.  Open Watcom
 * provides unsigned long long for 16-bit targets with integer-only
 * helpers, which libc16 supplies.
 */

typedef unsigned long long dfsize_t;

struct dfsizes {
    dfsize_t total;         /* bytes */
    dfsize_t used;
    dfsize_t avail;
    int pct;                /* percent used, rounded up; -1 if no capacity */
};

/* Byte counts from a free-space query.  Free clusters are clamped to
 * the total, so an inconsistent answer never underflows. */
void df_sizes(unsigned long spc, unsigned long bps, unsigned long avail,
              unsigned long total, struct dfsizes *s);

/* v in decimal.  buf needs 21 bytes. */
void fmt_u64(char *buf, dfsize_t v);

/* A compact binary size: "512B", "640K", "15.7M", "1.9G", "2.0T".
 * K is whole KiB, larger units have one decimal.  up nonzero rounds up
 * (so a nonzero size never shows as zero), else down.  buf needs 24
 * bytes. */
void fmt_size(char *buf, dfsize_t bytes, int up);

/* The same for a count of KiB; 0 is "0K". */
void fmt_kb(char *buf, unsigned long kb, int up);

/* A BCD version word as "M.mm" (0300h -> "3.00", 0201h -> "2.01"). */
void fmt_bcdver(char *buf, unsigned v);

/* major.minor with two minor digits ("4.00", "7.10"). */
void fmt_ver(char *buf, unsigned major, unsigned minor);

#endif
