#ifndef PROBE_H
#define PROBE_H

/* The probes.  DOS only. */

#include "sysinfo.h"

/* Fills si.  all: also query A:, B:, removable and CD-ROM drives. */
void probe_all(struct sysinfo *si, int all);

/* The DOS version alone, and the GR-DOS check (for --logo choice and
 * tests).  Part of probe_all. */
void probe_dos(struct dosinfo *d);

/* The video adapter and mode.  Part of probe_all. */
void probe_video(struct vidinfo *v);

/* Whether standard output is the console (IOCTL 4400h). */
int stdout_is_console(void);

#endif
