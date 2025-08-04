/*
 * (C)opyright 2025 Olexandr Sydorchuk
 * See LICENSE file for license details.
 *
 * Font management module for dzen2
 */

#include "font.h"
#include "kvstore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FONT "-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*"

/* External references */
extern void eprint(const char *errstr, ...);

/* Global font state */
static Display       *g_display       = NULL;
static int            g_screen        = 0;
static Fnt           *g_current_font  = NULL;
static Fnt           *g_default_font  = NULL;
static KeyValueStore *g_font_cache    = NULL;
static KeyValueStore *g_preload_cache = NULL;

#ifdef HAVE_XFT
/* XftDraw cache - maps drawable to XftDraw */
static KeyValueStore *g_xftdraw_cache = NULL;
/* XftColor cache - maps color string to XftColor */
static KeyValueStore *g_xftcolor_cache = NULL;
static Visual        *g_visual         = NULL;
static Colormap       g_colormap;
#endif

/* Forward declarations */
static void font_destroy_item(void *value);
static Fnt *font_load(const char *fontstr);
static void font_free(Fnt *font);
#ifdef HAVE_XFT
static void xftdraw_destroy_item(void *value);
static void xftcolor_destroy_item(void *value);
#endif

/* Initialize font subsystem */
void font_init(Display *dpy, int screen) {
    g_display = dpy;
    g_screen  = screen;

    /* Initialize font cache with destructor */
    g_font_cache = kvstore_create(font_destroy_item, NULL);
    if (!g_font_cache) {
        eprint("font: failed to create font cache\n");
    }

    /* Initialize preload cache with string destructor */
    g_preload_cache = kvstore_create(free, NULL);
    if (!g_preload_cache) {
        eprint("font: failed to create preload cache\n");
    }

#ifdef HAVE_XFT
    g_visual   = DefaultVisual(dpy, screen);
    g_colormap = DefaultColormap(dpy, screen);

    /* Initialize XftDraw cache with destructor */
    g_xftdraw_cache = kvstore_create(xftdraw_destroy_item, NULL);
    if (!g_xftdraw_cache) {
        eprint("font: failed to create XftDraw cache\n");
    }

    /* Initialize XftColor cache with destructor */
    g_xftcolor_cache = kvstore_create(xftcolor_destroy_item, NULL);
    if (!g_xftcolor_cache) {
        eprint("font: failed to create XftColor cache\n");
    }
#endif
}

/* Clean up all font resources */
void font_cleanup(void) {
    /* Destroy caches - this will call destructors for all cached fonts */
    if (g_font_cache) {
        kvstore_destroy(g_font_cache);
        g_font_cache = NULL;
    }

    if (g_preload_cache) {
        kvstore_destroy(g_preload_cache);
        g_preload_cache = NULL;
    }

    /* Clean up global references */
    g_current_font = NULL;
    g_default_font = NULL;

#ifdef HAVE_XFT
    if (g_xftdraw_cache) {
        kvstore_destroy(g_xftdraw_cache);
        g_xftdraw_cache = NULL;
    }

    if (g_xftcolor_cache) {
        kvstore_destroy(g_xftcolor_cache);
        g_xftcolor_cache = NULL;
    }
#endif
}

/* Font destructor for kvstore */
static void font_destroy_item(void *value) {
    if (!value)
        return;
    font_free((Fnt *)value);
}

/* Free a font structure */
static void font_free(Fnt *font) {
    if (!font)
        return;

#ifdef HAVE_XFT
    if (font->xftfont) {
        XftFontClose(g_display, font->xftfont);
        font->xftfont = NULL;
    }
#endif

    if (font->set) {
        XFreeFontSet(g_display, font->set);
        font->set = NULL;
    } else if (font->xfont) {
        XFreeFont(g_display, font->xfont);
        font->xfont = NULL;
    }

    free(font);
}

/* Load a font (internal function) */
static Fnt *font_load(const char *fontstr) {
    if (!fontstr || !*fontstr)
        return NULL;

    Fnt *font = calloc(1, sizeof(Fnt));
    if (!font)
        return NULL;

    const char *actual_fontstr = fontstr;
    int         xft_only       = 0;
    int         x11_only       = 0;

    /* Check for prefixes */
    if (strncmp(fontstr, "xft:", 4) == 0) {
        xft_only       = 1;
        actual_fontstr = fontstr + 4;
    } else if (strncmp(fontstr, "x:", 2) == 0) {
        x11_only       = 1;
        actual_fontstr = fontstr + 2;
    }

#ifdef HAVE_XFT
    /* Try XFT first if not x11_only */
    if (!x11_only) {
        font->xftfont = XftFontOpenName(g_display, g_screen, actual_fontstr);
        if (font->xftfont) {
            font->ascent  = font->xftfont->ascent;
            font->descent = font->xftfont->descent;
            font->height  = font->ascent + font->descent;
            return font;
        }

        /* If xft_only, fail here */
        if (xft_only) {
            free(font);
            return NULL;
        }
    }
#endif

    /* Try X11 core fonts */
    char *def, **missing;
    int   n;

    missing   = NULL;
    font->set = XCreateFontSet(g_display, actual_fontstr, &missing, &n, &def);
    if (missing)
        XFreeStringList(missing);

    if (font->set) {
        XFontSetExtents *font_extents;
        XFontStruct    **xfonts;
        char           **font_names;

        font_extents = XExtentsOfFontSet(font->set);
        n            = XFontsOfFontSet(font->set, &xfonts, &font_names);

        font->ascent  = 0;
        font->descent = 0;

        for (int i = 0; i < n; i++) {
            if (font->ascent < xfonts[i]->ascent)
                font->ascent = xfonts[i]->ascent;
            if (font->descent < xfonts[i]->descent)
                font->descent = xfonts[i]->descent;
        }

        font->height = font->ascent + font->descent;
        return font;
    }

    /* Try loading as simple X font */
    font->xfont = XLoadQueryFont(g_display, actual_fontstr);
    if (font->xfont) {
        font->ascent  = font->xfont->ascent;
        font->descent = font->xfont->descent;
        font->height  = font->ascent + font->descent;
        return font;
    }

    /* Failed to load font */
    free(font);
    return NULL;
}

/* Set the current font */
Fnt *font_set(const char *fontstr) {
    if (!fontstr || !*fontstr) {
        /* Empty string means reset to default */
        font_reset_to_default();
        return g_current_font;
    }

    /* Check if it's a numeric alias or dfnt alias */
    if (isdigit(fontstr[0]) || strncmp(fontstr, "dfnt", 4) == 0) {
        int index = -1;

        if (isdigit(fontstr[0])) {
            index = atoi(fontstr);
        } else if (strncmp(fontstr, "dfnt", 4) == 0 && isdigit(fontstr[4])) {
            index = atoi(fontstr + 4);
        }

        if (index >= 0) {
            char key[32];
            snprintf(key, sizeof(key), "%d", index);
            char *preloaded_name = kvstore_get(g_preload_cache, key);
            if (preloaded_name) {
                fontstr = preloaded_name;
            }
        }
    }

    /* Check if font is already cached */
    Fnt *font = kvstore_get(g_font_cache, fontstr);
    if (!font) {
        /* Load and cache the font */
        font = font_load(fontstr);
        if (font) {
            if (kvstore_set(g_font_cache, fontstr, font) != 0) {
                font_free(font);
                return NULL;
            }
        } else {
            eprint("font: cannot load font: '%s'\n", fontstr);
            return NULL;
        }
    }

    g_current_font = font;
    return font;
}

/* Set the default font */
void font_set_default(const char *fontstr) {
    if (!fontstr || !*fontstr) {
        fontstr = FONT;
    }

    Fnt *font = font_set(fontstr);
    if (font) {
        g_default_font = font;
    }
}

/* Get the current font */
Fnt *font_get_current(void) {
    if (!g_current_font && g_default_font) {
        g_current_font = g_default_font;
    }
    return g_current_font;
}

/* Reset to default font */
void font_reset_to_default(void) {
    if (g_default_font) {
        g_current_font = g_default_font;
    }
}

/* Calculate text width */
unsigned int font_get_text_width(const char *text, unsigned int len) {
    Fnt *font = font_get_current();
    if (!font)
        return 0;

#ifdef HAVE_XFT
    if (font->xftfont) {
        XGlyphInfo extents;
        XftTextExtentsUtf8(g_display, font->xftfont, (const FcChar8 *)text, len, &extents);
        return extents.xOff;
    }
#endif

    if (font->set) {
        XRectangle r;
        XmbTextExtents(font->set, text, len, NULL, &r);
        return r.width;
    }

    if (font->xfont) {
        return XTextWidth(font->xfont, text, len);
    }

    return 0;
}

#ifdef HAVE_XFT
/* XftDraw destructor */
static void xftdraw_destroy_item(void *value) {
    if (!value)
        return;
    XftDrawDestroy((XftDraw *)value);
}

/* XftColor destructor */
static void xftcolor_destroy_item(void *value) {
    if (!value)
        return;
    XftColor *color = (XftColor *)value;
    XftColorFree(g_display, g_visual, g_colormap, color);
    free(color);
}

/* Get or create XftDraw for drawable */
static XftDraw *get_xftdraw(Drawable drawable) {
    char key[32];
    snprintf(key, sizeof(key), "%lu", drawable);

    XftDraw *xftdraw = kvstore_get(g_xftdraw_cache, key);
    if (!xftdraw) {
        xftdraw = XftDrawCreate(g_display, drawable, g_visual, g_colormap);
        if (xftdraw) {
            kvstore_set(g_xftdraw_cache, key, xftdraw);
        }
    }
    return xftdraw;
}

/* Get or create XftColor */
static XftColor *get_xftcolor(const char *colorname) {
    if (!colorname || !*colorname)
        return NULL;

    XftColor *color = kvstore_get(g_xftcolor_cache, colorname);
    if (!color) {
        color = malloc(sizeof(XftColor));
        if (color) {
            if (XftColorAllocName(g_display, g_visual, g_colormap, colorname, color)) {
                kvstore_set(g_xftcolor_cache, colorname, color);
            } else {
                free(color);
                color = NULL;
            }
        }
    }
    return color;
}
#endif

/* Draw text at specified position */
void font_draw_text(Drawable drawable, GC gc, int x, int y, const char *text, unsigned int len, int reverse,
                    const char *fg_color, const char *bg_color) {
    Fnt *font = font_get_current();
    if (!font)
        return;

#ifdef HAVE_XFT
    if (font->xftfont) {
        /* Use XFT rendering */
        XftDraw *xftdraw = get_xftdraw(drawable);
        if (!xftdraw)
            return;

        /* Determine which color to use */
        const char *color_str = reverse ? bg_color : fg_color;
        if (!color_str) {
            color_str = reverse ? "#000000" : "#FFFFFF"; /* Default colors */
        }

        XftColor *xftcolor = get_xftcolor(color_str);
        if (!xftcolor)
            return;

        /* Draw text */
        XftDrawStringUtf8(xftdraw, xftcolor, font->xftfont, x, y + font->ascent, (const FcChar8 *)text, len);
        return;
    }
#endif

    /* Use X11 core font rendering */
    if (font->set) {
        XmbDrawString(g_display, drawable, font->set, gc, x, y + font->ascent, text, len);
    } else if (font->xfont) {
        /* Set font in GC for X11 core fonts */
        XGCValues gcv;
        gcv.font = font->xfont->fid;
        XChangeGC(g_display, gc, GCFont, &gcv);
        XDrawString(g_display, drawable, gc, x, y + font->ascent, text, len);
    }
}

/* Preload fonts with aliases */
void font_preload(const char *fonts) {
    if (!fonts || !*fonts)
        return;

    char *fonts_copy = strdup(fonts);
    if (!fonts_copy)
        return;

    int   index = 0;
    char *token = strtok(fonts_copy, ",");

    while (token != NULL && index < 64) {
        /* Trim whitespace */
        while (*token && isspace(*token))
            token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end))
            *end-- = '\0';

        if (*token) {
            /* Store the font name with numeric key */
            char key[32];
            snprintf(key, sizeof(key), "%d", index);

            /* Duplicate the font name for storage */
            char *font_name = strdup(token);
            if (font_name) {
                kvstore_set(g_preload_cache, key, font_name);

                /* Also preload the font into cache */
                font_set(token);
                index++;
            }
        }

        token = strtok(NULL, ",");
    }

    free(fonts_copy);
}

/* Get font height information */
void font_get_dimensions(int *ascent, int *descent, int *height) {
    Fnt *font = font_get_current();
    if (!font) {
        if (ascent)
            *ascent = 0;
        if (descent)
            *descent = 0;
        if (height)
            *height = 0;
        return;
    }

    if (ascent)
        *ascent = font->ascent;
    if (descent)
        *descent = font->descent;
    if (height)
        *height = font->height;
}

/* Compatibility functions for legacy code */
void setfont(const char *fontstr) {
    font_set(fontstr);
}

unsigned int textnw(Fnt *font, const char *text, unsigned int len) {
    /* Note: the font parameter is ignored, we use the current font */
    return font_get_text_width(text, len);
}