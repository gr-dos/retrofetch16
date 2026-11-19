#include <string.h>
#include "describe.h"
#include "format.h"
#include "out.h"

/* ------------------------------------------------------------------ */
/* helpers */

static char *cat(char *buf, const char *s)
{
    return strcat(buf, s);
}

static void cat_u(char *buf, unsigned long v)
{
    char tmp[24];

    fmt_u64(tmp, v);
    strcat(buf, tmp);
}

static void cat_hex2(char *buf, unsigned v)
{
    static const char hex[] = "0123456789ABCDEF";
    char tmp[4];

    tmp[0] = hex[v >> 4 & 15];
    tmp[1] = hex[v & 15];
    tmp[2] = 'h';
    tmp[3] = '\0';
    strcat(buf, tmp);
}

static void cat_hex4(char *buf, unsigned v)
{
    static const char hex[] = "0123456789ABCDEF";
    char tmp[6];

    tmp[0] = hex[v >> 12 & 15];
    tmp[1] = hex[v >> 8 & 15];
    tmp[2] = hex[v >> 4 & 15];
    tmp[3] = hex[v & 15];
    tmp[4] = 'h';
    tmp[5] = '\0';
    strcat(buf, tmp);
}

/* Copies at most n-1 bytes of s, with leading spaces dropped and runs
 * of spaces squeezed to one, and trailing spaces removed. */
static void squeeze(char *dst, const char *s, size_t n)
{
    size_t k = 0;
    int space = 0;

    while (*s == ' ')
        s++;
    for (; *s != '\0' && k + 1 < n; s++) {
        if (*s == ' ') {
            space = 1;
            continue;
        }
        if (space && k + 2 < n)
            dst[k++] = ' ';
        space = 0;
        dst[k++] = *s;
    }
    dst[k] = '\0';
}

/* ------------------------------------------------------------------ */
/* CPU and FPU */

static const char *queue_name(unsigned q, const char *four, const char *six,
                              const char *either)
{
    return q == 4 ? four : q == 6 ? six : either;
}

static unsigned cpuid_family(unsigned long sig)
{
    unsigned f = (unsigned)(sig >> 8 & 15);

    if (f == 15)
        f += (unsigned)(sig >> 20 & 255);
    return f;
}

static unsigned cpuid_model(unsigned long sig)
{
    unsigned f = (unsigned)(sig >> 8 & 15);
    unsigned m = (unsigned)(sig >> 4 & 15);

    if (f == 6 || f == 15)
        m += (unsigned)(sig >> 16 & 15) << 4;
    return m;
}

static void cpuid_name(const struct cpuinfo *c, char *buf)
{
    unsigned f = cpuid_family(c->sig), m = cpuid_model(c->sig);
    const char *v = c->vendor, *n = NULL;

    if (strcmp(v, "GenuineIntel") == 0) {
        if (f == 4)
            n = "Intel 486";
        else if (f == 5)
            n = m == 4 || m == 8 ? "Intel Pentium MMX" : "Intel Pentium";
        else if (f == 6)
            n = m == 1 ? "Intel Pentium Pro"
              : m == 3 || m == 5 ? "Intel Pentium II"
              : m == 6 ? "Intel Celeron"
              : m == 7 || m == 8 || m == 10 || m == 11 ? "Intel Pentium III"
              : "Intel P6-family";
        else if (f == 15)
            n = "Intel Pentium 4-family";
        else
            n = "Intel";
    } else if (strcmp(v, "AuthenticAMD") == 0) {
        if (f == 4)
            n = "AMD Am486/Am5x86";
        else if (f == 5)
            n = m < 4 ? "AMD K5" : m < 8 ? "AMD K6" : m == 8 ? "AMD K6-2"
              : "AMD K6-III";
        else if (f == 6)
            n = "AMD Athlon-family";
        else
            n = "AMD";
    } else if (strcmp(v, "CyrixInstead") == 0) {
        n = f == 4 ? "Cyrix 5x86" : f == 5 ? "Cyrix 6x86"
          : f == 6 ? "Cyrix 6x86MX/MII" : "Cyrix";
    } else if (strcmp(v, "CentaurHauls") == 0) {
        n = f == 5 ? "IDT WinChip" : f == 6 ? "VIA C3-family" : "Centaur";
    } else if (strcmp(v, "NexGenDriven") == 0) {
        n = "NexGen Nx586";
    } else if (strcmp(v, "UMC UMC UMC ") == 0) {
        n = "UMC U5";
    } else if (strcmp(v, "RiseRiseRise") == 0) {
        n = "Rise mP6";
    } else if (strcmp(v, "Geode by NSC") == 0) {
        n = "NSC Geode";
    } else if (strcmp(v, "GenuineTMx86") == 0) {
        n = "Transmeta";
    } else if (strcmp(v, "Vortex86 SoC") == 0) {
        n = "DM&P Vortex86";
    }
    if (n != NULL) {
        strcpy(buf, n);
    } else {
        squeeze(buf, v, 13);
        if (buf[0] == '\0')
            strcpy(buf, "unknown vendor");
    }
    /* A name that is only a vendor gets the family, to say something. */
    if (n == NULL || strchr(n, ' ') == NULL) {
        cat(buf, " family ");
        cat_u(buf, f);
        cat(buf, " model ");
        cat_u(buf, m);
    }
}

void cpu_name(const struct cpuinfo *c, char *buf)
{
    buf[0] = '\0';
    switch (c->cls) {
    case CPU_8086:
        if (c->nec)
            cat(buf, queue_name(c->queue, "NEC V20", "NEC V30",
                                "NEC V20/V30"));
        else
            cat(buf, queue_name(c->queue, "Intel 8088", "Intel 8086",
                                "8088/8086"));
        break;
    case CPU_186:
        cat(buf, queue_name(c->queue, "80188", "80186", "80186/80188"));
        break;
    case CPU_286:
        cat(buf, "80286");
        break;
    case CPU_386:
        cat(buf, "80386 (SX/DX not told apart)");
        break;
    case CPU_486:
        cat(buf, "486-class (no CPUID)");
        break;
    default:
        if (c->brand[0] != '\0')
            squeeze(buf, c->brand, NAME_SIZE - 24);
        else
            cpuid_name(c, buf);
        break;
    }
    if (c->mhz != 0) {
        cat(buf, " @ ~");
        cat_u(buf, c->mhz);
        cat(buf, " MHz");
    }
    if (c->v86)
        cat(buf, " (V86 mode)");
}

void fpu_name(const struct cpuinfo *c, char *buf)
{
    buf[0] = '\0';
    switch (c->fpu) {
    case FPU_NONE:
        cat(buf, c->bios_fpu ? "none found (BIOS reports one)" : "none");
        return;
    case FPU_PROJ:
        cat(buf, c->cls <= CPU_186 ? "8087" : "80287");
        return;
    }
    /* Affine only: 387-based.  On a 186 that is the 80187, on a 286
     * the 80287XL (or a 387-compatible for the 286). */
    if (c->cls == CPU_8086)
        cat(buf, "387-class");
    else if (c->cls == CPU_186)
        cat(buf, "80187");
    else if (c->cls == CPU_286)
        cat(buf, "80287XL-class");
    else if (c->cls == CPU_386)
        cat(buf, "80387");
    else if (c->cls == CPU_486)
        cat(buf, "x87, integrated or 487");
    else if (c->feat & 1)
        cat(buf, "x87, integrated");
    else
        cat(buf, "x87, external");
}

/* ------------------------------------------------------------------ */
/* DOS */

static const struct { unsigned char id; const char *name; } oems[] = {
    { 0x00, "IBM" },
    { 0x01, "Compaq" },
    { 0x02, "MS Packaged Product" },
    { 0x04, "AT&T" },
    { 0x05, "Zenith" },
    { 0x06, "Hewlett-Packard" },
    { 0x0d, "Packard-Bell" },
    { 0x16, "DEC" },
    { 0x23, "Olivetti" },
    { 0x29, "Toshiba" },
    { 0x5e, "RxDOS" },
    { 0x66, "PhysTechSoft" },
    { 0xee, "DR DOS" },
    { 0xef, "Novell" },
    { 0xfd, "FreeDOS" },
    { 0xff, "Microsoft" },
};

void oem_name(unsigned oem, char *buf)
{
    unsigned i;

    for (i = 0; i < sizeof oems / sizeof oems[0]; i++)
        if (oems[i].id == oem) {
            strcpy(buf, oems[i].name);
            return;
        }
    buf[0] = '\0';
    cat_hex2(buf, oem);
}

static const struct { unsigned code; const char *name; } drs[] = {
    { 0x1060, "DOS Plus" },
    { 0x1063, "DR DOS 3.41" },
    { 0x1064, "DR DOS 3.42" },
    { 0x1065, "DR DOS 5.0" },
    { 0x1067, "DR DOS 6.0" },
    { 0x1070, "PalmDOS" },
    { 0x1071, "DR DOS 6.0" },
    { 0x1072, "Novell DOS 7" },
};

void os_name(const struct dosinfo *d, char *buf)
{
    char v[12];
    unsigned i;

    buf[0] = '\0';
    switch (d->kind) {
    case OS_GRDOS:
        cat(buf, "GR-DOS ");
        fmt_ver(v, d->grmajor, d->grminor);
        cat(buf, v);
        return;
    case OS_FREEDOS:
        cat(buf, "FreeDOS");
        return;
    case OS_DRDOS:
        for (i = 0; i < sizeof drs / sizeof drs[0]; i++)
            if (drs[i].code == d->drcode) {
                cat(buf, drs[i].name);
                return;
            }
        cat(buf, "DR DOS family");
        return;
    }
    cat(buf, "DOS ");
    fmt_ver(v, d->major, d->minor);
    cat(buf, v);
    cat(buf, " (OEM: ");
    oem_name(d->oem, v);
    cat(buf, v);
    cat(buf, ")");
}

void kernel_name(const struct dosinfo *d, char *buf)
{
    char v[12];

    buf[0] = '\0';
    if (d->kind == OS_FREEDOS && d->kernel[0] != '\0') {
        char *p;

        squeeze(buf, d->kernel, NAME_SIZE - 16);
        /* "FreeDOS kernel 2043 (build 2043 OEM:0xfd) [compiled ...]" */
        if ((p = strchr(buf, '(')) != NULL || (p = strchr(buf, '[')) != NULL) {
            while (p > buf && p[-1] == ' ')
                p--;
            *p = '\0';
        }
        cat(buf, ", ");
    }
    cat(buf, "DOS ");
    fmt_ver(v, d->major, d->minor);
    cat(buf, v);
    if (d->has_true && (d->tmajor != d->major || d->tminor != d->minor)) {
        cat(buf, " (true ");
        fmt_ver(v, d->tmajor, d->tminor);
        cat(buf, v);
        cat(buf, ")");
    }
    if (d->kind == OS_GRDOS) {
        cat(buf, ", GR-DOS API rev ");
        cat_u(buf, d->grrev);
    } else if (d->kind == OS_DRDOS) {
        cat(buf, ", DR code ");
        cat_hex4(buf, d->drcode);
    }
}

/* ------------------------------------------------------------------ */
/* machine and video */

void machine_name(unsigned model, const char *biosdate, char *buf)
{
    const char *n;

    switch (model) {
    case 0xff: n = "PC-class"; break;
    case 0xfe: case 0xfb: n = "PC/XT-class"; break;
    case 0xfd: n = "PCjr"; break;
    case 0xfc: n = "AT-class"; break;
    case 0xfa: n = "PS/2 Model 30"; break;
    case 0xf9: n = "PC Convertible"; break;
    case 0xf8: n = "PS/2 (MCA)"; break;
    default: n = NULL; break;
    }
    buf[0] = '\0';
    if (n != NULL) {
        cat(buf, n);
    } else {
        cat(buf, "model ");
        cat_hex2(buf, model);
    }
    if (biosdate[0] != '\0') {
        cat(buf, ", BIOS ");
        cat(buf, biosdate);
    }
}

static const char *const adapters[] = {
    "MDA", "Hercules", "CGA", "EGA", "MCGA", "VGA"
};

void video_name(const struct vidinfo *v, char *buf)
{
    char tmp[12];

    strcpy(buf, v->adapter <= VID_VGA ? adapters[v->adapter] : "unknown");
    if (v->vbe != 0) {
        cat(buf, ", VBE ");
        fmt_bcdver(tmp, v->vbe);
        /* "2.00" -> "2.0" */
        if (tmp[strlen(tmp) - 1] == '0')
            tmp[strlen(tmp) - 1] = '\0';
        cat(buf, tmp);
        if (v->vbe_kb != 0) {
            cat(buf, " ");
            fmt_kb(tmp, v->vbe_kb, 0);
            cat(buf, tmp);
        }
    }
    cat(buf, ", ");
    cat_u(buf, v->cols);
    cat(buf, "x");
    cat_u(buf, v->rows);
    cat(buf, v->mono ? " mono" : " color");
}

/* ------------------------------------------------------------------ */
/* drives */

#define BAR 10

void drive_value(const struct driveinfo *d, char *buf)
{
    char used[24], total[24];
    int i, fill;

    buf[0] = '\0';
    if (d->state == DRV_SKIPPED) {
        cat(buf, d->flags & DRVF_CDROM ? "CD-ROM" : "removable");
        cat(buf, ", not queried");
        return;
    }
    if (d->state != DRV_OK) {
        cat(buf, "not ready");
        return;
    }
    fmt_size(used, d->sizes.used, 1);
    fmt_size(total, d->sizes.total, 1);
    cat(buf, used);
    cat(buf, " / ");
    cat(buf, total);
    if (d->sizes.pct < 0) {
        cat(buf, " (-)");
    } else {
        cat(buf, " (");
        cat_u(buf, (unsigned long)d->sizes.pct);
        cat(buf, "%)");
    }
    if (d->flags & DRVF_REMOTE)
        cat(buf, " net");
    if (d->sizes.pct < 0)
        return;
    cat(buf, " ");
    cat(buf, d->sizes.pct >= 90 ? C_LRED : d->sizes.pct >= 70 ? C_YELLOW
             : C_LGREEN);
    fill = (d->sizes.pct * BAR + 99) / 100;
    for (i = 0; i < BAR; i++) {
        if (i == fill)
            cat(buf, C_DGRAY);
        cat(buf, i < fill ? "\xdb" : "\xb0");
    }
}

/* ------------------------------------------------------------------ */
/* the report */

static char *add(struct report *r, int accent, const char *label)
{
    char *p;
    char mk[3];

    if (r->n >= REPORT_LINES)
        r->n = REPORT_LINES - 1;
    p = r->line[r->n++];
    mk[0] = MK;
    mk[1] = (char)(accent < 10 ? '0' + accent : 'a' + accent - 10);
    mk[2] = '\0';
    strcpy(p, mk);
    if (label != NULL) {
        cat(p, label);
        cat(p, ": " C_GRAY);
    }
    return p;
}

/* Appends s, but no more than fits in a report line. */
static void addval(char *p, const char *s)
{
    size_t n = strlen(p), m = strlen(s);

    if (n + m >= REPORT_SIZE)
        m = REPORT_SIZE - 1 - n;
    memcpy(p + n, s, m);
    p[n + m] = '\0';
}

void describe(const struct sysinfo *si, int accent, struct report *r)
{
    char buf[NAME_SIZE * 2], a[24], b[24];
    const struct meminfo *m = &si->mem;
    char *p;
    int i, n;

    r->n = 0;

    /* title and rule */
    p = add(r, accent, NULL);
    addval(p, si->host[0] != '\0' ? si->host : "PC");
    n = (int)strlen(si->host[0] != '\0' ? si->host : "PC");
    p = add(r, COLOR_DEFAULT, NULL);
    for (i = 0; i < n; i++)
        addval(p, "-");

    os_name(&si->dos, buf);
    addval(add(r, accent, "OS"), buf);
    kernel_name(&si->dos, buf);
    addval(add(r, accent, "Kernel"), buf);
    if (si->dos.winmajor != 0) {
        buf[0] = '\0';
        fmt_ver(buf, si->dos.winmajor, si->dos.winminor);
        cat(buf, " (enhanced mode)");
        addval(add(r, accent, "Windows"), buf);
    }
    machine_name(si->model, si->biosdate, buf);
    addval(add(r, accent, "Machine"), buf);
    cpu_name(&si->cpu, buf);
    addval(add(r, accent, "CPU"), buf);
    fpu_name(&si->cpu, buf);
    addval(add(r, accent, "FPU"), buf);

    /* memory */
    fmt_size(a, m->avail, 0);
    fmt_kb(b, m->conv_kb, 0);
    p = add(r, accent, "Memory");
    addval(p, a);
    addval(p, " free / ");
    addval(p, b);
    if (m->xms) {
        p = add(r, accent, "XMS");
        fmt_kb(a, m->xms_free_kb, 0);
        addval(p, a);
        addval(p, " free");
        if (m->ext_kb != 0) {
            fmt_kb(b, m->ext_kb, 0);
            addval(p, " / ");
            addval(p, b);
        }
        fmt_bcdver(a, m->xms_ver);
        addval(p, " (v");
        addval(p, a);
        addval(p, ")");
    } else if (si->cpu.cls >= CPU_286) {
        p = add(r, accent, "Extended");
        if (m->ext_kb != 0) {
            fmt_kb(a, m->ext_kb, 0);
            addval(p, a);
            addval(p, " (no XMS driver)");
        } else {
            addval(p, "none");
        }
    }
    if (m->ems) {
        p = add(r, accent, "EMS");
        fmt_kb(a, m->ems_free_kb, 0);
        fmt_kb(b, m->ems_total_kb, 0);
        addval(p, a);
        addval(p, " free / ");
        addval(p, b);
        a[0] = (char)('0' + (m->ems_ver >> 4 & 15) % 10);
        a[1] = '.';
        a[2] = (char)('0' + (m->ems_ver & 15) % 10);
        a[3] = '\0';
        addval(p, " (v");
        addval(p, a);
        addval(p, ")");
    }

    video_name(&si->vid, buf);
    addval(add(r, accent, "Video"), buf);
    if (si->vid.oem[0] != '\0') {
        squeeze(buf, si->vid.oem, sizeof buf);
        addval(add(r, accent, "GPU"), buf);
    }
    if (si->shell[0] != '\0')
        addval(add(r, accent, "Shell"), si->shell);
    p = add(r, accent, "Floppy");
    if (si->floppies == 0) {
        addval(p, "none");
    } else {
        a[0] = '\0';
        cat_u(a, si->floppies);
        addval(p, a);
        addval(p, si->floppies == 1 ? " drive" : " drives");
    }

    for (i = 0; i < si->ndrives; i++) {
        const struct driveinfo *d = &si->drives[i];
        char label[12];

        strcpy(label, "Disk (?:)");
        label[6] = (char)('A' + d->letter);
        drive_value(d, buf);
        addval(add(r, accent, label), buf);
    }
}
