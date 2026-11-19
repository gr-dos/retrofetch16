/*
 * The probes: BIOS, DOS and hardware calls.  DOS only.
 *
 * Order matters for safety.  Nothing newer than the 8086 runs before
 * cpu_probe has shown the CPU has it (cpu.asm), INT 15h memory calls
 * run only on a 286 or later, INT 2Fh only on DOS 3 or later with a
 * nonzero vector, and the floppy letters A: and B: and removable or
 * CD-ROM drives are never queried unless asked for.
 */

#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include "probe.h"

/* cpu.asm */
unsigned __cdecl cpu_probe(void);
unsigned __cdecl cpu_queue(void);
unsigned __cdecl fpu_probe(void);
void __cdecl cpu_cpuid(unsigned long leaf, unsigned long *regs);
void __cdecl cpu_rdtsc(unsigned long *tsc);
int __cdecl xms_probe(unsigned long *out, int big);

/* One byte from an I/O port. */
unsigned char port_in(unsigned port);
#pragma aux port_in = "in al, dx" parm [dx] value [al] modify exact [al];

typedef unsigned char __far *farbytes;

static farbytes far_ptr(unsigned seg, unsigned off)
{
    return (farbytes)MK_FP(seg, off);
}

/* Copies a far string of at most n-1 bytes, stopping at a control
 * character. */
static void far_copy(char *dst, farbytes src, unsigned n)
{
    unsigned k = 0;

    while (k + 1 < n && src[k] >= ' ') {
        dst[k] = (char)src[k];
        k++;
    }
    dst[k] = '\0';
}

/* Whether 8 far bytes are s. */
static int far_eq8(farbytes p, const char *s)
{
    int i;

    for (i = 0; i < 8; i++)
        if (p[i] != (unsigned char)s[i])
            return 0;
    return 1;
}

static int vector_set(unsigned intno)
{
    unsigned __far *v = (unsigned __far *)MK_FP(0, intno * 4);

    return v[0] != 0 || v[1] != 0;
}

/* ------------------------------------------------------------------ */
/* CPU */

static void cpuid_str(unsigned long leaf, char *buf)
{
    unsigned long r[4];

    cpu_cpuid(leaf, r);
    memcpy(buf, r, 16);
}

/* Waits for the BIOS tick count (18.2 Hz) to change.  0 if it does
 * not within a generous bound (interrupts off, or a stopped timer). */
static int next_tick(void)
{
    volatile unsigned long __far *ticks =
        (volatile unsigned long __far *)MK_FP(0x40, 0x6c);
    unsigned long t = *ticks, start[2], now[2], n = 0;

    cpu_rdtsc(start);
    while (*ticks == t) {
        if (++n < 50000000UL)
            continue;
        cpu_rdtsc(now);
        /* 2^28 cycles is many ticks at any clock the loop count did not
         * already cover. */
        if (now[1] != start[1] || now[0] - start[0] > 0x10000000UL)
            return 0;
    }
    return 1;
}

#define MHZ_TICKS 3

/* The time-stamp counter's rate over three BIOS ticks (about 165 ms),
 * in MHz, or 0. */
static unsigned long tsc_mhz(void)
{
    unsigned long a[2], b[2];
    unsigned long long d;
    int i;

    if (!next_tick())
        return 0;
    cpu_rdtsc(a);
    for (i = 0; i < MHZ_TICKS; i++)
        if (!next_tick())
            return 0;
    cpu_rdtsc(b);
    d = ((unsigned long long)b[1] << 32 | b[0]) -
        ((unsigned long long)a[1] << 32 | a[0]);
    /* A tick is 65536 / 1193182 s. */
    d = d * 1193182UL / 65536UL / MHZ_TICKS;
    return (unsigned long)((d + 500000UL) / 1000000UL);
}

static void probe_cpu(struct cpuinfo *c)
{
    union REGS r;
    unsigned p = cpu_probe();

    memset(c, 0, sizeof *c);
    c->cls = p & 0xff;
    c->nec = (p & 0x100) != 0;
    c->v86 = (p & 0x200) != 0;
    if (c->cls <= CPU_186)
        c->queue = cpu_queue();
    c->fpu = fpu_probe();
    int86(0x11, &r, &r);
    c->bios_fpu = (r.x.ax & 2) != 0;

    if (c->cls == CPU_CPUID) {
        unsigned long v[4];

        cpu_cpuid(0, v);
        c->maxleaf = v[0];
        memcpy(c->vendor, &v[1], 4);
        memcpy(c->vendor + 4, &v[3], 4);
        memcpy(c->vendor + 8, &v[2], 4);
        c->vendor[12] = '\0';
        if (c->maxleaf >= 1) {
            cpu_cpuid(1, v);
            c->sig = v[0];
            c->feat = v[3];
        }
        cpu_cpuid(0x80000000UL, v);
        if (v[0] >= 0x80000004UL && v[0] <= 0x8000ffffUL) {
            cpuid_str(0x80000002UL, c->brand);
            cpuid_str(0x80000003UL, c->brand + 16);
            cpuid_str(0x80000004UL, c->brand + 32);
            c->brand[48] = '\0';
        }
        if (c->feat & 0x10)             /* TSC */
            c->mhz = tsc_mhz();
    }
}

/* ------------------------------------------------------------------ */
/* DOS */

/* One GR-DOS identification call (gr-dos docs/grver.md): nonzero if
 * the answer passes every check, with the version in *ver and the
 * revision in *rev. */
static int grdos_call(unsigned nonce, unsigned *ver, unsigned *rev)
{
    union REGS r;

    r.x.ax = 0x3347;
    r.x.bx = 0x4752;
    r.x.dx = nonce;
    r.x.cx = 0;
    int86(0x21, &r, &r);
    if (r.x.ax != 0x4752 || r.x.dx != (unsigned)~nonce || r.h.cl < 1 ||
        r.h.bl > 99)
        return 0;
    *ver = r.x.bx;
    *rev = r.h.cl;
    return 1;
}

void probe_dos(struct dosinfo *d)
{
    union REGS r;
    unsigned v1, v2, rev1, rev2;

    memset(d, 0, sizeof *d);
    r.x.ax = 0x3000;
    r.x.bx = 0;
    int86(0x21, &r, &r);
    d->major = r.h.al;
    d->minor = r.h.ah;
    d->oem = r.h.bh;
    if (d->major == 0) {                /* DOS 1.x returns AL=0 */
        d->major = 1;
        return;
    }

    if (d->major >= 5) {
        r.x.ax = 0x3306;
        r.x.bx = 0;
        int86(0x21, &r, &r);
        if (r.h.al != 0xff && r.h.bl != 0) {
            d->has_true = 1;
            d->tmajor = r.h.bl;
            d->tminor = r.h.bh;
        }
    }

    /* GR-DOS: two nonces, and the answers must agree. */
    if (grdos_call(0x5a3c, &v1, &rev1) && grdos_call(0xc3a5, &v2, &rev2) &&
        v1 == v2 && rev1 == rev2) {
        d->kind = OS_GRDOS;
        d->grmajor = v1 >> 8;
        d->grminor = v1 & 0xff;
        d->grrev = rev1;
    } else {
        /* DR DOS 3.41 and later: AX=4452h ("DR") answers with carry
         * clear and a version code 10xxh; elsewhere it is an invalid
         * IOCTL subfunction. */
        r.x.ax = 0x4452;
        r.x.bx = 0;
        int86(0x21, &r, &r);
        if (!r.x.cflag && (r.x.ax & 0xff00) == 0x1000) {
            d->kind = OS_DRDOS;
            d->drcode = r.x.ax;
        } else if (d->oem == 0xfd) {
            d->kind = OS_FREEDOS;
            /* FreeDOS: AX=33FFh gives DX:AX, its kernel version string. */
            r.x.ax = 0x33ff;
            r.x.dx = 0;
            int86(0x21, &r, &r);
            if (r.x.dx != 0)
                far_copy(d->kernel, far_ptr(r.x.dx, r.x.ax),
                         sizeof d->kernel);
        }
    }

    /* Windows enhanced mode (INT 2Fh AX=1600h), DOS 3 and later. */
    if (d->major >= 3 && vector_set(0x2f)) {
        r.x.ax = 0x1600;
        int86(0x2f, &r, &r);
        if (r.h.al == 0x01 || r.h.al == 0xff) {
            d->winmajor = 2;            /* Windows/386 2.x */
        } else if (r.h.al != 0x00 && r.h.al != 0x80) {
            d->winmajor = r.h.al;
            d->winminor = r.h.ah;
        }
    }
}

/* ------------------------------------------------------------------ */
/* memory */

static void probe_mem(struct meminfo *m, const struct cpuinfo *c,
                      const struct dosinfo *d)
{
    union REGS r;
    unsigned top;

    memset(m, 0, sizeof *m);
    int86(0x12, &r, &r);
    m->conv_kb = r.x.ax;

    /* The block DOS gave this program: from the PSP to the segment in
     * PSP offset 2.  It is what any program could have. */
    top = *(unsigned __far *)MK_FP(_psp, 2);
    if (top > _psp)
        m->avail = (unsigned long)(top - _psp) * 16;

    if (c->cls >= CPU_286) {
        r.x.ax = 0xe801;
        r.x.bx = r.x.cx = r.x.dx = 0;
        int86(0x15, &r, &r);
        if (!r.x.cflag && (r.x.ax | r.x.bx | r.x.cx | r.x.dx) != 0) {
            if (r.x.ax != 0 || r.x.bx != 0)
                m->ext_kb = r.x.ax + (unsigned long)r.x.bx * 64;
            else
                m->ext_kb = r.x.cx + (unsigned long)r.x.dx * 64;
        } else {
            r.h.ah = 0x88;
            int86(0x15, &r, &r);
            if (!r.x.cflag)
                m->ext_kb = r.x.ax;
        }
    }

    if (d->major >= 3 && vector_set(0x2f)) {
        unsigned long x[3];

        if (xms_probe(x, c->cls >= CPU_386)) {
            m->xms = 1;
            m->xms_ver = (unsigned)x[0];
            m->xms_largest_kb = x[1];
            m->xms_free_kb = x[2];
        }
    }

    if (vector_set(0x67)) {
        unsigned __far *v = (unsigned __far *)MK_FP(0, 0x67 * 4);

        if (far_eq8(far_ptr(v[1], 10), "EMMXXXX0")) {
            r.h.ah = 0x40;
            int86(0x67, &r, &r);
            if (r.h.ah == 0) {
                m->ems = 1;
                r.h.ah = 0x46;
                int86(0x67, &r, &r);
                if (r.h.ah == 0)
                    m->ems_ver = r.h.al;
                r.h.ah = 0x42;
                int86(0x67, &r, &r);
                if (r.h.ah == 0) {
                    m->ems_free_kb = (unsigned long)r.x.bx * 16;
                    m->ems_total_kb = (unsigned long)r.x.dx * 16;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* video */

/* Bit 7 of the Hercules status port toggles with vertical retrace;
 * an MDA's does not. */
static int hercules(void)
{
    unsigned char first = port_in(0x3ba) & 0x80;
    unsigned n;

    for (n = 0; n < 0x8000; n++)
        if ((port_in(0x3ba) & 0x80) != first)
            return 1;
    return 0;
}

static unsigned char vbe_buf[512];

void probe_video(struct vidinfo *v)
{
    union REGS r;
    struct SREGS s;
    farbytes bda = far_ptr(0x40, 0);

    memset(v, 0, sizeof *v);
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    v->mode = r.h.al & 0x7f;
    v->cols = r.h.ah;
    v->rows = 25;

    r.x.ax = 0x1a00;
    r.x.bx = 0;
    int86(0x10, &r, &r);
    if (r.h.al == 0x1a) {
        switch (r.h.bl) {
        case 0x01: v->adapter = VID_MDA; v->mono = 1; break;
        case 0x02: v->adapter = VID_CGA; break;
        case 0x04: v->adapter = VID_EGA; break;
        case 0x05: v->adapter = VID_EGA; v->mono = 1; break;
        case 0x07: v->adapter = VID_VGA; v->mono = 1; break;
        case 0x0b: v->adapter = VID_MCGA; v->mono = 1; break;
        case 0x0c: v->adapter = VID_MCGA; break;
        default: v->adapter = VID_VGA; break;
        }
    } else {
        r.h.ah = 0x12;
        r.h.bl = 0x10;
        r.h.bh = 0xff;
        int86(0x10, &r, &r);
        if (r.h.bl != 0x10) {
            v->adapter = VID_EGA;
            v->mono = r.h.bh == 1;
        } else {
            int86(0x11, &r, &r);
            if ((r.x.ax & 0x30) == 0x30) {
                v->adapter = hercules() ? VID_HERC : VID_MDA;
                v->mono = 1;
            } else {
                v->adapter = VID_CGA;
            }
        }
    }
    if (v->mode == 7)
        v->mono = 1;
    if (v->adapter >= VID_EGA && bda[0x84] >= 24 && bda[0x84] < 60)
        v->rows = bda[0x84] + 1u;

    if (v->adapter == VID_VGA) {
        memset(vbe_buf, 0, sizeof vbe_buf);
        memcpy(vbe_buf, "VBE2", 4);
        segread(&s);
        s.es = s.ds;
        r.x.ax = 0x4f00;
        r.x.di = (unsigned)vbe_buf;
        int86x(0x10, &r, &r, &s);
        if (r.x.ax == 0x004f && memcmp(vbe_buf, "VESA", 4) == 0) {
            unsigned *w = (unsigned *)vbe_buf;

            v->vbe = w[2];
            v->vbe_kb = w[9] * 64;
            if (w[4] != 0 || w[3] != 0)
                far_copy(v->oem, far_ptr(w[4], w[3]), sizeof v->oem);
        }
    }
}

/* ------------------------------------------------------------------ */
/* drives */

/* Answers DOS's critical-error prompt with Fail, so that a query of a
 * drive that is not ready fails instead of asking Abort, Retry, Fail. */
static int __cdecl __far fail_handler(unsigned deverror, unsigned errcode,
                                      unsigned __far *devhdr)
{
    (void)deverror;
    (void)errcode;
    (void)devhdr;
    return _HARDERR_FAIL;
}

static int extended_free(int drive, struct dfsizes *s)
{
    static unsigned char buf[44];
    char root[4];
    union REGS r;
    struct SREGS sr;
    unsigned long *l = (unsigned long *)(buf + 4);

    root[0] = (char)('A' + drive);
    root[1] = ':';
    root[2] = '\\';
    root[3] = '\0';
    memset(buf, 0, sizeof buf);
    segread(&sr);
    sr.es = sr.ds;
    r.x.ax = 0x7303;
    r.x.dx = (unsigned)root;
    r.x.di = (unsigned)buf;
    r.x.cx = sizeof buf;
    int86x(0x21, &r, &r, &sr);
    if (r.x.cflag || r.x.ax == 0x7300 || l[0] == 0 || l[1] == 0)
        return -1;
    /* +04 sectors per cluster, +08 bytes per sector, +0C free clusters,
     * +10 total clusters */
    df_sizes(l[0], l[1], l[2], l[3], s);
    return 0;
}

static int classic_free(int drive, struct dfsizes *s)
{
    union REGS r;

    r.h.ah = 0x36;
    r.h.dl = (unsigned char)(drive + 1);
    int86(0x21, &r, &r);
    if (r.x.ax == 0xffff)
        return -1;
    df_sizes(r.x.ax, r.x.cx, r.x.bx, r.x.dx, s);
    return 0;
}

static void probe_drives(struct sysinfo *si, int all)
{
    const struct dosinfo *d = &si->dos;
    union REGS r;
    int last, drive, cdrom = 0, ioctl = d->major > 3 ||
        (d->major == 3 && d->minor >= 10);

    si->ndrives = 0;
    if (ioctl)
        _harderr(fail_handler);

    r.h.ah = 0x19;
    int86(0x21, &r, &r);
    r.h.dl = r.h.al;
    r.h.ah = 0x0e;                      /* select the current drive */
    int86(0x21, &r, &r);
    last = r.h.al > MAX_DRIVES ? MAX_DRIVES : r.h.al;

    /* MSCDEX (INT 2Fh AX=1500h): the number of CD-ROM drives. */
    if (d->major >= 3 && vector_set(0x2f)) {
        r.x.ax = 0x1500;
        r.x.bx = 0;
        int86(0x2f, &r, &r);
        cdrom = r.x.bx != 0;
    }

    for (drive = all ? 0 : 2; drive < last; drive++) {
        struct driveinfo *di = &si->drives[si->ndrives];
        int valid = 0;

        di->letter = drive;
        di->state = DRV_OK;
        di->flags = 0;

        if (cdrom) {
            r.x.ax = 0x150b;
            r.x.cx = (unsigned)drive;
            r.x.bx = 0;
            int86(0x2f, &r, &r);
            if (r.x.bx == 0xadad && r.x.ax != 0)
                di->flags |= DRVF_CDROM;
        }
        if (ioctl && !(di->flags & DRVF_CDROM)) {
            /* 4409h: is the drive remote?  Fails with 0Fh for a letter
             * that is not in use.  Neither call touches the medium. */
            r.x.ax = 0x4409;
            r.h.bl = (unsigned char)(drive + 1);
            int86(0x21, &r, &r);
            if (r.x.cflag && r.x.ax == 0x0f)
                continue;
            if (!r.x.cflag) {
                valid = 1;
                if (r.x.dx & 0x8000)    /* SUBST: another drive's space */
                    continue;
                if (r.x.dx & 0x1000)
                    di->flags |= DRVF_REMOTE;
            }
            if (!(di->flags & DRVF_REMOTE)) {
                r.x.ax = 0x4408;
                r.h.bl = (unsigned char)(drive + 1);
                int86(0x21, &r, &r);
                if (!r.x.cflag && r.x.ax == 0)
                    di->flags |= DRVF_REMOVABLE;
            }
        }
        if (!all && (di->flags & (DRVF_CDROM | DRVF_REMOVABLE))) {
            di->state = DRV_SKIPPED;
            si->ndrives++;
            continue;
        }
        if (d->major >= 7 && extended_free(drive, &di->sizes) == 0) {
            di->flags |= DRVF_EXTENDED;
        } else if (classic_free(drive, &di->sizes) != 0) {
            /* Without IOCTL a missing letter and a failed query look
             * the same: skip it, as a gap in the drive letters. */
            if (!valid && !(di->flags & DRVF_CDROM))
                continue;
            di->state = DRV_NOTREADY;
        }
        si->ndrives++;
    }
}

/* ------------------------------------------------------------------ */

int stdout_is_console(void)
{
    union REGS r;

    r.x.ax = 0x4400;
    r.x.bx = 1;
    int86(0x21, &r, &r);
    /* a character device that is the standard output device */
    return !r.x.cflag && (r.x.dx & 0x82) == 0x82;
}

static int bios_date(farbytes p)
{
    int i;

    for (i = 0; i < 8; i++)
        if (i == 2 || i == 5 ? p[i] != '/' : p[i] < '0' || p[i] > '9')
            return 0;
    return 1;
}

void probe_all(struct sysinfo *si, int all)
{
    union REGS r;
    struct SREGS s;
    farbytes rom = far_ptr(0xf000, 0xfff5);
    const char *comspec;

    memset(si, 0, sizeof *si);
    probe_cpu(&si->cpu);
    probe_dos(&si->dos);
    probe_mem(&si->mem, &si->cpu, &si->dos);
    probe_video(&si->vid);

    si->model = rom[9];
    if (bios_date(rom))
        far_copy(si->biosdate, rom, sizeof si->biosdate);

    int86(0x11, &r, &r);
    if (r.x.ax & 1)
        si->floppies = (r.x.ax >> 6 & 3) + 1;

    if ((comspec = getenv("COMSPEC")) != NULL) {
        strncpy(si->shell, comspec, sizeof si->shell - 1);
        si->shell[sizeof si->shell - 1] = '\0';
    }

    /* The network machine name (AX=5E00h, DOS 3.1 and later): valid if
     * CH is nonzero.  Fifteen characters padded with spaces. */
    if (si->dos.major > 3 || (si->dos.major == 3 && si->dos.minor >= 10)) {
        char name[16];
        int n;

        memset(name, 0, sizeof name);
        segread(&s);
        r.x.ax = 0x5e00;
        r.x.dx = (unsigned)name;
        r.x.cx = 0;
        int86x(0x21, &r, &r, &s);
        if (!r.x.cflag && r.h.ch != 0) {
            name[15] = '\0';
            for (n = 15; n > 0 && (name[n - 1] == ' ' || name[n - 1] == '\0');
                 n--)
                name[n - 1] = '\0';
            strcpy(si->host, name);
        }
    }

    probe_drives(si, all);
}
