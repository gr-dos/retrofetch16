/*
 * retrofch - system information for DOS, Fastfetch style
 *
 * An 8086-compatible report of the CPU, FPU, DOS, memory, video and
 * disks beside an OS logo (Open Watcom C with libc16).  Inspired by
 * Leah Neukirchen's dosfetch; see README.md, "Provenance".
 */

#include <stdio.h>
#include "describe.h"
#include "layout.h"
#include "logo.h"
#include "options.h"
#include "out.h"
#include "probe.h"
#include "video.h"

#ifndef VERSION
#define VERSION   "unknown"
#endif

static void usage(void)
{
    fputs(
        "Usage: retrofch [OPTION]...\n"
        "Show system information beside a DOS logo.\n"
        "\n"
        "  -a, --all      also query A:, B:, removable and CD-ROM drives\n"
        "      --plain    no colors\n"
        "      --ansi     colors as ANSI escapes (ANSI.SYS, or a file)\n"
        "      --mono     monochrome attributes on the screen\n"
        "      --ascii    7-bit ASCII instead of code page 437 graphics\n"
        "      --logo=L   logo: grdos, dos or none (default: by kernel)\n"
        "  -h, --help     display this help and exit (also -?, /?)\n"
        "      --version  output version information and exit\n"
        "\n"
        "Colors go to the screen through the BIOS; redirected output is\n"
        "plain text unless --ansi is given.\n",
        stdout);
}

static int finish(void)
{
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fputs("retrofch: write error\n", stderr);
        return 2;
    }
    return 0;
}

static struct sysinfo si;
static struct report rep;
static char obuf[BUFSIZ];

int main(int argc, char **argv)
{
    struct options opt;
    struct out o;
    const struct logo *lg;
    int width = 80, palette = 0;

    switch (parse_options(argc, argv, &opt, stderr)) {
    case OPT_HELP:
        usage();
        return finish();
    case OPT_VERSION:
        fputs("retrofch (retrofetch16) " VERSION "\n", stdout);
        return finish();
    case OPT_USAGE:
        return 1;
    }

    probe_all(&si, opt.all);

    if (opt.logo < 0)
        opt.logo = si.dos.kind == OS_GRDOS ? LOGO_GRDOS : LOGO_DOS;
    lg = logo_get(opt.logo);

    if (opt.mode == MODE_ANSI) {
        out_init(&o, OUT_ANSI, stdout);
        palette = 1;
    } else if ((opt.mode == MODE_AUTO || opt.mode == MODE_MONO) &&
               stdout_is_console()) {
        video_out(&o);
        o.mono = opt.mode == MODE_MONO || si.vid.mono ||
                 si.vid.mode == 0 || si.vid.mode == 2;
        palette = !o.mono;
        if (si.vid.cols >= 40)
            width = (int)si.vid.cols;
    } else {
        out_init(&o, OUT_PLAIN, stdout);
    }
    o.ascii = opt.ascii;
    if (o.kind != OUT_BIOS)
        setvbuf(stdout, obuf, _IOFBF, sizeof obuf);

    describe(&si, lg->accent, &rep);
    render(&o, lg, &rep, palette, width);
    return finish();
}
