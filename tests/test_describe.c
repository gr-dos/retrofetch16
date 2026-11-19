/* Host unit tests for src/describe.c: naming from raw probe results */

#include <string.h>
#include "describe.h"
#include "out.h"
#include "unit.h"

static int str_is(const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("  got \"%s\", want \"%s\"\n", got, want);
        return 0;
    }
    return 1;
}

/* The markup with color codes removed. */
static const char *plain(const char *s)
{
    static char buf[REPORT_SIZE];
    char *p = buf;

    while (*s != '\0') {
        if (*s == MK) {
            if (*++s != '\0')
                s++;
        } else {
            *p++ = *s++;
        }
    }
    *p = '\0';
    return buf;
}

static const char *cpu(unsigned cls, int nec, unsigned queue)
{
    static char buf[NAME_SIZE];
    struct cpuinfo c;

    memset(&c, 0, sizeof c);
    c.cls = cls;
    c.nec = nec;
    c.queue = queue;
    cpu_name(&c, buf);
    return buf;
}

static const char *fpu(unsigned cls, unsigned fpu, int bios, unsigned long feat)
{
    static char buf[NAME_SIZE];
    struct cpuinfo c;

    memset(&c, 0, sizeof c);
    c.cls = cls;
    c.fpu = fpu;
    c.bios_fpu = bios;
    c.feat = feat;
    fpu_name(&c, buf);
    return buf;
}

static const char *cpuid(const char *vendor, unsigned long sig,
                         const char *brand)
{
    static char buf[NAME_SIZE];
    struct cpuinfo c;

    memset(&c, 0, sizeof c);
    c.cls = CPU_CPUID;
    strcpy(c.vendor, vendor);
    c.sig = sig;
    strcpy(c.brand, brand);
    cpu_name(&c, buf);
    return buf;
}

/* Finds the report line whose text (without markup) starts with s. */
static const char *line(const struct report *r, const char *s)
{
    int i;

    for (i = 0; i < r->n; i++)
        if (strncmp(plain(r->line[i]), s, strlen(s)) == 0)
            return plain(r->line[i]);
    return "";
}

int main(void)
{
    char buf[NAME_SIZE * 2];
    struct cpuinfo c;
    struct dosinfo d;
    struct vidinfo v;
    struct driveinfo dr;
    struct sysinfo si;
    struct report r;

    /* CPU classes; the prefetch queue tells 8088 from 8086 */
    CHECK(str_is(cpu(CPU_8086, 0, 4), "Intel 8088"));
    CHECK(str_is(cpu(CPU_8086, 0, 6), "Intel 8086"));
    CHECK(str_is(cpu(CPU_8086, 0, 0), "8088/8086"));
    CHECK(str_is(cpu(CPU_8086, 1, 4), "NEC V20"));
    CHECK(str_is(cpu(CPU_8086, 1, 6), "NEC V30"));
    CHECK(str_is(cpu(CPU_8086, 1, 0), "NEC V20/V30"));
    CHECK(str_is(cpu(CPU_186, 0, 4), "80188"));
    CHECK(str_is(cpu(CPU_186, 0, 6), "80186"));
    CHECK(str_is(cpu(CPU_186, 0, 0), "80186/80188"));
    CHECK(str_is(cpu(CPU_286, 0, 0), "80286"));
    CHECK(str_is(cpu(CPU_386, 0, 0), "80386 (SX/DX not told apart)"));
    CHECK(str_is(cpu(CPU_486, 0, 0), "486-class (no CPUID)"));

    /* CPUID: the brand string wins, squeezed */
    CHECK(str_is(cpuid("GenuineIntel", 0x0683, "      Intel(R) Pentium(R) III  "
                       "processor"), "Intel(R) Pentium(R) III processor"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0435, ""), "Intel 486"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0525, ""), "Intel Pentium"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0543, ""), "Intel Pentium MMX"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0619, ""), "Intel Pentium Pro"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0652, ""), "Intel Pentium II"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0f29, ""), "Intel Pentium 4-family"));
    CHECK(str_is(cpuid("AuthenticAMD", 0x0562, ""), "AMD K6"));
    CHECK(str_is(cpuid("AuthenticAMD", 0x0580, ""), "AMD K6-2"));
    CHECK(str_is(cpuid("CyrixInstead", 0x0520, ""), "Cyrix 6x86"));
    CHECK(str_is(cpuid("GenuineIntel", 0x0300, ""),
                 "Intel family 3 model 0"));
    CHECK(str_is(cpuid("NewVendorXYZ", 0x0512, ""),
                 "NewVendorXYZ family 5 model 1"));
    CHECK(str_is(cpuid("", 0x0400, ""), "unknown vendor family 4 model 0"));
    /* extended family and model */
    CHECK(str_is(cpuid("AuthenticAMD", 0x00a00f11UL, ""),
                 "AMD family 25 model 1"));

    /* MHz and V86 mode */
    memset(&c, 0, sizeof c);
    c.cls = CPU_CPUID;
    strcpy(c.vendor, "GenuineIntel");
    c.sig = 0x0525;
    c.mhz = 75;
    c.v86 = 1;
    cpu_name(&c, buf);
    CHECK(str_is(buf, "Intel Pentium @ ~75 MHz (V86 mode)"));

    /* FPUs */
    CHECK(str_is(fpu(CPU_8086, FPU_NONE, 0, 0), "none"));
    CHECK(str_is(fpu(CPU_8086, FPU_NONE, 1, 0),
                 "none found (BIOS reports one)"));
    CHECK(str_is(fpu(CPU_8086, FPU_PROJ, 1, 0), "8087"));
    CHECK(str_is(fpu(CPU_186, FPU_PROJ, 1, 0), "8087"));
    CHECK(str_is(fpu(CPU_286, FPU_PROJ, 1, 0), "80287"));
    CHECK(str_is(fpu(CPU_386, FPU_PROJ, 1, 0), "80287"));
    CHECK(str_is(fpu(CPU_386, FPU_AFFINE, 1, 0), "80387"));
    CHECK(str_is(fpu(CPU_8086, FPU_AFFINE, 1, 0), "387-class"));
    CHECK(str_is(fpu(CPU_186, FPU_AFFINE, 1, 0), "80187"));
    CHECK(str_is(fpu(CPU_286, FPU_AFFINE, 1, 0), "80287XL-class"));
    CHECK(str_is(fpu(CPU_486, FPU_AFFINE, 1, 0), "x87, integrated or 487"));
    CHECK(str_is(fpu(CPU_CPUID, FPU_AFFINE, 1, 1), "x87, integrated"));
    CHECK(str_is(fpu(CPU_CPUID, FPU_AFFINE, 1, 0), "x87, external"));

    /* DOS: GR-DOS by its call, not by version */
    memset(&d, 0, sizeof d);
    d.major = 4;
    d.oem = 0xff;
    os_name(&d, buf);
    CHECK(str_is(buf, "DOS 4.00 (OEM: Microsoft)"));
    kernel_name(&d, buf);
    CHECK(str_is(buf, "DOS 4.00"));
    d.kind = OS_GRDOS;
    d.grmajor = 0;
    d.grminor = 4;
    d.grrev = 1;
    os_name(&d, buf);
    CHECK(str_is(buf, "GR-DOS 0.04"));
    kernel_name(&d, buf);
    CHECK(str_is(buf, "DOS 4.00, GR-DOS API rev 1"));

    memset(&d, 0, sizeof d);
    d.major = 7;
    d.minor = 10;
    d.oem = 0xfd;
    d.kind = OS_FREEDOS;
    /* as probe_dos copies it: the first 47 bytes */
    strcpy(d.kernel, "FreeDOS kernel 2043 (build 2043 OEM:0xfd) [comp");
    os_name(&d, buf);
    CHECK(str_is(buf, "FreeDOS"));
    kernel_name(&d, buf);
    CHECK(str_is(buf, "FreeDOS kernel 2043, DOS 7.10"));

    memset(&d, 0, sizeof d);
    d.major = 6;
    d.oem = 0xee;
    d.kind = OS_DRDOS;
    d.drcode = 0x1072;
    os_name(&d, buf);
    CHECK(str_is(buf, "Novell DOS 7"));
    kernel_name(&d, buf);
    CHECK(str_is(buf, "DOS 6.00, DR code 1072h"));
    d.drcode = 0x1099;
    os_name(&d, buf);
    CHECK(str_is(buf, "DR DOS family"));

    memset(&d, 0, sizeof d);
    d.major = 5;
    d.oem = 0x42;
    d.has_true = 1;
    d.tmajor = 7;
    d.tminor = 10;
    os_name(&d, buf);
    CHECK(str_is(buf, "DOS 5.00 (OEM: 42h)"));
    kernel_name(&d, buf);
    CHECK(str_is(buf, "DOS 5.00 (true 7.10)"));
    d.tmajor = 5;
    d.tminor = 0;
    kernel_name(&d, buf);
    CHECK(str_is(buf, "DOS 5.00"));
    oem_name(0x00, buf);
    CHECK(str_is(buf, "IBM"));

    /* machine */
    machine_name(0xfe, "01/10/86", buf);
    CHECK(str_is(buf, "PC/XT-class, BIOS 01/10/86"));
    machine_name(0xfc, "", buf);
    CHECK(str_is(buf, "AT-class"));
    machine_name(0x12, "", buf);
    CHECK(str_is(buf, "model 12h"));

    /* video */
    memset(&v, 0, sizeof v);
    v.adapter = VID_HERC;
    v.mono = 1;
    v.cols = 80;
    v.rows = 25;
    video_name(&v, buf);
    CHECK(str_is(buf, "Hercules, 80x25 mono"));
    v.adapter = VID_VGA;
    v.mono = 0;
    v.rows = 50;
    v.vbe = 0x0200;
    v.vbe_kb = 4096;
    video_name(&v, buf);
    CHECK(str_is(buf, "VGA, VBE 2.0 4.0M, 80x50 color"));
    v.vbe = 0x0102;
    v.vbe_kb = 512;
    video_name(&v, buf);
    CHECK(str_is(buf, "VGA, VBE 1.02 512K, 80x50 color"));

    /* drives */
    memset(&dr, 0, sizeof dr);
    dr.letter = 2;
    dr.state = DRV_SKIPPED;
    dr.flags = DRVF_CDROM;
    drive_value(&dr, buf);
    CHECK(str_is(buf, "CD-ROM, not queried"));
    dr.flags = DRVF_REMOVABLE;
    drive_value(&dr, buf);
    CHECK(str_is(buf, "removable, not queried"));
    dr.state = DRV_NOTREADY;
    drive_value(&dr, buf);
    CHECK(str_is(buf, "not ready"));
    dr.state = DRV_OK;
    dr.flags = 0;
    df_sizes(4, 512, 75, 100, &dr.sizes);       /* 25% used */
    drive_value(&dr, buf);
    CHECK(str_is(plain(buf), "50K / 200K (25%) \xdb\xdb\xdb\xb0\xb0\xb0\xb0"
                             "\xb0\xb0\xb0"));
    CHECK(strstr(buf, C_LGREEN) != NULL);
    df_sizes(4, 512, 5, 100, &dr.sizes);        /* 95%: red */
    drive_value(&dr, buf);
    CHECK(strstr(buf, C_LRED) != NULL);
    CHECK(str_is(plain(buf), "190K / 200K (95%) \xdb\xdb\xdb\xdb\xdb\xdb"
                             "\xdb\xdb\xdb\xdb"));
    dr.flags = DRVF_REMOTE;
    df_sizes(1, 512, 0, 0, &dr.sizes);
    drive_value(&dr, buf);
    CHECK(str_is(buf, "0B / 0B (-) net"));

    /* the whole report: an 8088 XT with no FPU, no extended memory */
    memset(&si, 0, sizeof si);
    si.cpu.cls = CPU_8086;
    si.cpu.queue = 4;
    si.dos.major = 3;
    si.dos.minor = 30;
    si.dos.oem = 0;
    si.vid.adapter = VID_CGA;
    si.vid.cols = 80;
    si.vid.rows = 25;
    si.mem.conv_kb = 640;
    si.mem.avail = 580UL * 1024 + 100;
    si.model = 0xfe;
    strcpy(si.biosdate, "11/08/82");
    si.floppies = 2;
    strcpy(si.shell, "C:\\COMMAND.COM");
    si.ndrives = 1;
    si.drives[0].letter = 2;
    df_sizes(4, 512, 2000, 10000, &si.drives[0].sizes);
    describe(&si, 11, &r);
    CHECK(str_is(plain(r.line[0]), "PC"));
    CHECK(str_is(plain(r.line[1]), "--"));
    CHECK(strncmp(r.line[2], C_LCYAN "OS: " C_GRAY, 8) == 0);
    CHECK(str_is(line(&r, "OS:"), "OS: DOS 3.30 (OEM: IBM)"));
    CHECK(str_is(line(&r, "Machine:"), "Machine: PC/XT-class, BIOS 11/08/82"));
    CHECK(str_is(line(&r, "CPU:"), "CPU: Intel 8088"));
    CHECK(str_is(line(&r, "FPU:"), "FPU: none"));
    CHECK(str_is(line(&r, "Memory:"), "Memory: 580K free / 640K"));
    CHECK(str_is(line(&r, "Extended:"), ""));   /* no line on an 8088 */
    CHECK(str_is(line(&r, "XMS:"), ""));
    CHECK(str_is(line(&r, "EMS:"), ""));
    CHECK(str_is(line(&r, "Video:"), "Video: CGA, 80x25 color"));
    CHECK(str_is(line(&r, "Shell:"), "Shell: C:\\COMMAND.COM"));
    CHECK(str_is(line(&r, "Floppy:"), "Floppy: 2 drives"));
    CHECK(strncmp(line(&r, "Disk (C:):"), "Disk (C:): 15.7M / 19.6M (80%)",
                  30) == 0);
    CHECK(r.n == 12);

    /* a 486 with HIMEM, EMM386 and Windows */
    si.cpu.cls = CPU_486;
    si.cpu.v86 = 1;
    si.mem.ext_kb = 7168;
    si.mem.xms = 1;
    si.mem.xms_ver = 0x0300;
    si.mem.xms_free_kb = 6000;
    si.mem.ems = 1;
    si.mem.ems_ver = 0x40;
    si.mem.ems_free_kb = 1024;
    si.mem.ems_total_kb = 2048;
    si.dos.winmajor = 3;
    si.dos.winminor = 10;
    strcpy(si.host, "RETRO");
    si.floppies = 0;
    describe(&si, 10, &r);
    CHECK(str_is(plain(r.line[0]), "RETRO"));
    CHECK(str_is(plain(r.line[1]), "-----"));
    CHECK(str_is(line(&r, "CPU:"), "CPU: 486-class (no CPUID) (V86 mode)"));
    CHECK(str_is(line(&r, "XMS:"), "XMS: 5.8M free / 7.0M (v3.00)"));
    CHECK(str_is(line(&r, "EMS:"), "EMS: 1.0M free / 2.0M (v4.0)"));
    CHECK(str_is(line(&r, "Windows:"), "Windows: 3.10 (enhanced mode)"));
    CHECK(str_is(line(&r, "Floppy:"), "Floppy: none"));

    /* a 286 without XMS */
    si.cpu.cls = CPU_286;
    si.cpu.v86 = 0;
    si.mem.xms = 0;
    describe(&si, 10, &r);
    CHECK(str_is(line(&r, "Extended:"), "Extended: 7.0M (no XMS driver)"));
    si.mem.ext_kb = 0;
    describe(&si, 10, &r);
    CHECK(str_is(line(&r, "Extended:"), "Extended: none"));

    /* lines never overflow */
    memset(si.shell, 'x', sizeof si.shell - 1);
    si.shell[sizeof si.shell - 1] = '\0';
    describe(&si, 10, &r);
    CHECK(strlen(line(&r, "Shell:")) == 7 + sizeof si.shell - 1);

    return UNIT_DONE();
}
