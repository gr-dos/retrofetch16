#ifndef VIDEO_H
#define VIDEO_H

#include "out.h"

/* Sets o up to write to the screen through the BIOS.  DOS only. */
void video_out(struct out *o);

#endif
