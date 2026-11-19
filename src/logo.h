#ifndef LOGO_H
#define LOGO_H

/* The left-hand artwork.  Portable. */

#define LOGO_NONE   0
#define LOGO_DOS    1       /* generic DOS: any kernel not identified */
#define LOGO_GRDOS  2       /* GR-DOS, when its identification call answers */

struct logo {
    const char *name;       /* for --logo= */
    int width;              /* visible columns of every line */
    int accent;             /* color of the report's labels */
    const char *const *lines;   /* markup (out.h), NULL-terminated */
};

/* The logo with this LOGO_* number; LOGO_NONE gives an empty one. */
const struct logo *logo_get(int which);

/* LOGO_* for a --logo= name ("dos", "grdos", "none"; any case), or -1. */
int logo_byname(const char *name);

#endif
