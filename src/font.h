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

/* Font management functions */
void         font_init(void);
void         font_cleanup(void);
void         setfont(const char *fontstr);
unsigned int textnw(Fnt *font, const char *text, unsigned int len);

/* Font preloading functions */
void font_preload(char *s);
void font_preload_single(const char *fontstr, int p);

/* Font cache functions */
#ifdef HAVE_XFT
void     font_cache_init(void);
void     font_cache_cleanup(void);
XftFont *get_cached_font(Display *display, int screen, const char *font_name);
#endif

#endif /* FONT_H */