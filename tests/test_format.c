/* Host unit tests for src/format.c */

#include <string.h>
#include "format.h"
#include "unit.h"

static int size_is(dfsize_t bytes, int up, const char *want)
{
    char buf[24];

    fmt_size(buf, bytes, up);
    if (strcmp(buf, want) != 0) {
        printf("  fmt_size(%llu, %d) = \"%s\", want \"%s\"\n", bytes, up,
               buf, want);
        return 0;
    }
    return 1;
}

static int kb_is(unsigned long kb, int up, const char *want)
{
    char buf[24];

    fmt_kb(buf, kb, up);
    if (strcmp(buf, want) != 0) {
        printf("  fmt_kb(%lu, %d) = \"%s\", want \"%s\"\n", kb, up, buf,
               want);
        return 0;
    }
    return 1;
}

static int str_is(const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("  got \"%s\", want \"%s\"\n", got, want);
        return 0;
    }
    return 1;
}

int main(void)
{
    struct dfsizes s;
    char buf[24];

    /* sizes */
    CHECK(size_is(0, 1, "0B"));
    CHECK(size_is(512, 1, "512B"));
    CHECK(size_is(1023, 0, "1023B"));
    CHECK(size_is(1024, 0, "1K"));
    CHECK(size_is(1025, 0, "1K"));
    CHECK(size_is(1025, 1, "2K"));
    CHECK(size_is(640UL * 1024, 0, "640K"));
    CHECK(size_is(1023UL * 1024, 0, "1023K"));
    CHECK(size_is(1023UL * 1024 + 1, 1, "1.0M"));    /* 1024K rounds to M */
    CHECK(size_is(1024UL * 1024, 0, "1.0M"));
    CHECK(size_is(1536UL * 1024, 0, "1.5M"));
    CHECK(size_is(1024UL * 1024 + 1, 1, "1.1M"));    /* never shows less */
    CHECK(size_is(1024UL * 1024 + 1, 0, "1.0M"));
    CHECK(size_is(122UL * 1024 * 1024 + 1024, 1, "122.1M"));
    CHECK(size_is(2047UL * 1024 * 1024, 0, "1.9G"));
    CHECK(size_is(2048ULL * 1024 * 1024, 0, "2.0G"));
    CHECK(size_is(1024ULL * 1024 * 1024 * 1024, 0, "1.0T"));
    CHECK(size_is(5000ULL * 1024 * 1024 * 1024 * 1024, 0, "5000.0T"));
    CHECK(size_is(~0ULL, 1, "16777216.0T"));

    CHECK(kb_is(0, 0, "0K"));
    CHECK(kb_is(640, 0, "640K"));
    CHECK(kb_is(15360, 0, "15.0M"));
    CHECK(kb_is(4194304UL, 0, "4.0G"));

    /* capacity: INT 21h AH=36h on a 2 GB FAT16 volume */
    df_sizes(64, 512, 30000, 65524, &s);
    CHECK(s.total == 65524ULL * 32768);
    CHECK(s.avail == 30000ULL * 32768);
    CHECK(s.used == 35524ULL * 32768);
    CHECK(s.pct == 55);                 /* 54.2 rounded up */
    /* free clamped to total */
    df_sizes(1, 512, 10, 5, &s);
    CHECK(s.used == 0 && s.avail == s.total && s.pct == 0);
    /* no capacity */
    df_sizes(1, 512, 0, 0, &s);
    CHECK(s.pct == -1);
    /* full */
    df_sizes(4, 512, 0, 100, &s);
    CHECK(s.pct == 100);
    /* AX=7303h counts on a large FAT32 volume: no overflow */
    df_sizes(64, 512, 1000000UL, 4000000000UL, &s);
    CHECK(s.total == 4000000000ULL * 32768);
    CHECK(s.pct == 100);                /* 99.975 rounded up */

    /* versions */
    fmt_bcdver(buf, 0x0300);
    CHECK(str_is(buf, "3.00"));
    fmt_bcdver(buf, 0x0201);
    CHECK(str_is(buf, "2.01"));
    fmt_bcdver(buf, 0x0102);
    CHECK(str_is(buf, "1.02"));
    fmt_bcdver(buf, 0x1000);
    CHECK(str_is(buf, "10.00"));
    fmt_ver(buf, 4, 0);
    CHECK(str_is(buf, "4.00"));
    fmt_ver(buf, 7, 10);
    CHECK(str_is(buf, "7.10"));
    fmt_ver(buf, 0, 4);
    CHECK(str_is(buf, "0.04"));
    fmt_u64(buf, 18446744073709551615ULL);
    CHECK(str_is(buf, "18446744073709551615"));

    return UNIT_DONE();
}
