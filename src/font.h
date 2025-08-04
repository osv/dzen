/*
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * (C)opyright 2025 Olexandr Sydorchuk
 * See LICENSE file for license details.
 *
 * Font management module for dzen2
 */

#ifndef FONT_H
#define FONT_H

#include "../config.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#endif

typedef struct Fnt {
    XFontStruct *xfont;
    XFontSet     set;
    int          ascent;
    int          descent;
    int          height;
#ifdef HAVE_XFT
    XftFont   *xftfont;
    XGlyphInfo extents;
    int        width;
#endif
} Fnt;

/* Initialize font subsystem */
void font_init(Display *dpy, int screen);

/* Clean up all font resources */
void font_cleanup(void);

/* Set the current font */
Fnt *font_set(const char *fontstr);

/* Set the default font */
void font_set_default(const char *fontstr);

/* Get the current font */
Fnt *font_get_current(void);

/* Reset to default font */
void font_reset_to_default(void);

/* Calculate text width */
unsigned int font_get_text_width(const char *text, unsigned int len);

/* Draw text at specified position with current font and colors */
void font_draw_text(Drawable drawable, GC gc, int x, int y, const char *text, unsigned int len, int reverse,
                    const char *fg_color, const char *bg_color);

/* Preload fonts with aliases */
void font_preload(const char *fonts);

/* Get font height information */
void font_get_dimensions(int *ascent, int *descent, int *height);

#endif /* FONT_H */