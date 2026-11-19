#include <string.h>
#include "format.h"

void df_sizes(unsigned long spc, unsigned long bps, unsigned long avail,
              unsigned long total, struct dfsizes *s)
{
    dfsize_t cluster = (dfsize_t)spc * bps;

    if (avail > total)
        avail = total;
    s->total = cluster * total;
    s->avail = cluster * avail;
    s->used = s->total - s->avail;
    if (s->total == 0)
        s->pct = -1;
    else
        s->pct = (int)((s->used * 100 + s->total - 1) / s->total);
}

void fmt_u64(char *buf, dfsize_t v)
{
    char tmp[21];
    int n = 0;

    do {
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v != 0);
    while (n > 0)
        *buf++ = tmp[--n];
    *buf = '\0';
}

static const char units[] = "KMGT";

void fmt_size(char *buf, dfsize_t bytes, int up)
{
    dfsize_t div = 1024, q;
    unsigned u;

    if (bytes < 1024) {
        fmt_u64(buf, bytes);
        strcat(buf, "B");
        return;
    }
    /* KiB: whole units.  Move up once the value would show as 1024. */
    q = bytes / 1024 + (up && bytes % 1024 != 0);
    if (q < 1024) {
        fmt_u64(buf, q);
        strcat(buf, "K");
        return;
    }
    /* Tenths of the unit.  Splitting off the remainder keeps bytes * 10
     * from overflowing for any 64-bit value. */
    for (u = 1, div = 1024UL * 1024; ; u++, div *= 1024) {
        q = bytes / div * 10;
        if (up)
            q += (bytes % div * 10 + div - 1) / div;
        else
            q += bytes % div * 10 / div;
        if (q < 10240 || u == sizeof units - 2)
            break;
    }
    fmt_u64(buf, q / 10);
    buf += strlen(buf);
    *buf++ = '.';
    *buf++ = (char)('0' + (int)(q % 10));
    *buf++ = units[u];
    *buf = '\0';
}

void fmt_kb(char *buf, unsigned long kb, int up)
{
    if (kb == 0) {
        strcpy(buf, "0K");
        return;
    }
    fmt_size(buf, (dfsize_t)kb * 1024, up);
}

void fmt_bcdver(char *buf, unsigned v)
{
    unsigned hi = v >> 8;

    if (hi >= 10)
        *buf++ = (char)('0' + hi / 16 % 10);
    *buf++ = (char)('0' + hi % 16 % 10);
    *buf++ = '.';
    *buf++ = (char)('0' + (v >> 4 & 15) % 10);
    *buf++ = (char)('0' + (v & 15) % 10);
    *buf = '\0';
}

void fmt_ver(char *buf, unsigned major, unsigned minor)
{
    fmt_u64(buf, major);
    buf += strlen(buf);
    *buf++ = '.';
    *buf++ = (char)('0' + minor / 10 % 10);
    *buf++ = (char)('0' + minor % 10);
    *buf = '\0';
}
