#ifndef DESCRIBE_H
#define DESCRIBE_H

/* Report lines from raw probe results.  Portable: no DOS calls. */

#include "sysinfo.h"

#define REPORT_LINES  44        /* 15 fixed lines and 26 drives */
#define REPORT_SIZE   160       /* bytes of markup per line */
#define NAME_SIZE     64        /* for the *_name functions */

struct report {
    int n;
    char line[REPORT_LINES][REPORT_SIZE];
};

/* The whole report: a title, a rule, then "Label: value" lines with
 * labels in the accent color. */
void describe(const struct sysinfo *si, int accent, struct report *r);

/* The parts, for the tests.  Each writes plain text (no markup). */
void cpu_name(const struct cpuinfo *c, char *buf);
void fpu_name(const struct cpuinfo *c, char *buf);
void os_name(const struct dosinfo *d, char *buf);
void kernel_name(const struct dosinfo *d, char *buf);
void machine_name(unsigned model, const char *biosdate, char *buf);
void video_name(const struct vidinfo *v, char *buf);
void oem_name(unsigned oem, char *buf);

/* A drive's value, as markup: sizes, percentage and a usage bar. */
void drive_value(const struct driveinfo *d, char *buf);

#endif
