#ifndef LAYOUT_H
#define LAYOUT_H

/* The logo beside the report.  Portable. */

#include "describe.h"
#include "logo.h"
#include "out.h"

#define LOGO_GAP 3          /* columns between the logo and the report */

/* Writes the logo's lines beside the report's, then, if palette, a
 * blank line and the 16 colors.  Lines are cut to width - 1 columns,
 * so that none wraps on a screen width columns wide. */
void render(struct out *o, const struct logo *lg, const struct report *r,
            int palette, int width);

#endif
