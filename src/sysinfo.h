#ifndef SYSINFO_H
#define SYSINFO_H

/*
 * What the probes found, as raw values.  probe.c (DOS only) fills a
 * struct sysinfo; describe.c (portable) turns it into report lines, so
 * the host unit tests can describe any machine without running on it.
 */

#include "format.h"

/* cpu_probe classes */
#define CPU_8086   0        /* 8086/8088 or NEC V20/V30 */
#define CPU_186    1        /* 80186/80188 */
#define CPU_286    2
#define CPU_386    3
#define CPU_486    4        /* 486-class without CPUID */
#define CPU_CPUID  5        /* has CPUID */

/* fpu_probe results */
#define FPU_NONE   0
#define FPU_PROJ   1        /* projective infinity: 8087 or 80287 */
#define FPU_AFFINE 2        /* affine only: 80387 or later */

struct cpuinfo {
    unsigned cls;           /* CPU_* */
    int nec;                /* NEC V20/V30 (class 0 only) */
    int v86;                /* virtual 8086 mode */
    unsigned queue;         /* prefetch queue bytes: 4, 6, or 0 if not
                             * observed; class 0 and 1 only */
    unsigned fpu;           /* FPU_* */
    int bios_fpu;           /* INT 11h says a coprocessor is installed */
    /* CPUID (class 5) */
    char vendor[13];
    unsigned long maxleaf;
    unsigned long sig;      /* leaf 1 EAX: family, model, stepping */
    unsigned long feat;     /* leaf 1 EDX */
    char brand[49];         /* leaves 80000002h-80000004h, or "" */
    unsigned long mhz;      /* TSC estimate, 0 if not measured */
};

/* DOS identification */
#define OS_UNKNOWN 0        /* not positively identified */
#define OS_GRDOS   1
#define OS_FREEDOS 2
#define OS_DRDOS   3

struct dosinfo {
    unsigned major, minor;  /* INT 21h AH=30h */
    unsigned oem;           /* BH from AH=30h */
    int has_true;           /* AX=3306h answered (DOS 5+) */
    unsigned tmajor, tminor;
    int kind;               /* OS_* */
    unsigned grmajor, grminor;  /* GR-DOS release (AX=3347h) */
    unsigned grrev;             /* its API revision */
    unsigned drcode;        /* DR DOS version code (AX=4452h) */
    char kernel[48];        /* FreeDOS kernel string (AX=33FFh), or "" */
    unsigned winmajor, winminor;    /* Windows enhanced mode, 0 if none */
};

/* Video adapters, most capable first */
#define VID_MDA    0
#define VID_HERC   1
#define VID_CGA    2
#define VID_EGA    3
#define VID_MCGA   4
#define VID_VGA    5

struct vidinfo {
    unsigned adapter;       /* VID_* */
    int mono;               /* monochrome display */
    unsigned mode;          /* current BIOS mode */
    unsigned cols, rows;
    unsigned vbe;           /* VESA BIOS version (BCD), 0 if none */
    unsigned vbe_kb;        /* its video memory, KB */
    char oem[40];           /* its OEM string, or "" */
};

struct meminfo {
    unsigned conv_kb;       /* INT 12h */
    unsigned long avail;    /* bytes in this program's block at load */
    unsigned long ext_kb;   /* BIOS extended memory (286+), 0 if none */
    int xms;                /* XMS driver present */
    unsigned xms_ver;       /* BCD */
    unsigned long xms_free_kb, xms_largest_kb;
    int ems;                /* EMS driver present */
    unsigned ems_ver;       /* BCD-ish: 40h is 4.0 */
    unsigned long ems_free_kb, ems_total_kb;
};

/* Drive states */
#define DRV_OK          0   /* capacity known */
#define DRV_SKIPPED     1   /* removable or CD-ROM, not queried */
#define DRV_NOTREADY    2   /* query failed */

#define DRVF_REMOTE     1
#define DRVF_REMOVABLE  2
#define DRVF_CDROM      4
#define DRVF_EXTENDED   8   /* sizes from AX=7303h */

struct driveinfo {
    int letter;             /* 0 = A: */
    int state;              /* DRV_* */
    unsigned flags;         /* DRVF_* */
    struct dfsizes sizes;   /* when DRV_OK */
};

#define MAX_DRIVES 26

struct sysinfo {
    struct cpuinfo cpu;
    struct dosinfo dos;
    struct vidinfo vid;
    struct meminfo mem;
    unsigned model;         /* BIOS model byte at F000:FFFEh */
    char biosdate[9];       /* F000:FFF5h, "mm/dd/yy", or "" */
    unsigned floppies;      /* INT 11h */
    char host[16];          /* machine name, or "" */
    char shell[64];         /* COMSPEC, or "" */
    int ndrives;
    struct driveinfo drives[MAX_DRIVES];
};

#endif
