/*
 * (C)opyright 2025 Olexandr Sydorchuk
 * See LICENSE file for license details.
 *
 * Font management module for dzen2
 */

#include "font.h"
#include "dzen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT "-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*"

/* External references */
extern Dzen dzen;
extern void eprint(const char *errstr, ...);

#ifdef HAVE_XFT
/* Shared cache structure for font caching */
typedef struct Cache {
    char         *key;
    void         *value;
    struct Cache *next;
} Cache;

static Cache *font_cache = NULL;

static void *get_cached_value(Cache **cache, const char *key) {
    Cache *current = *cache;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

static void add_to_cache(Cache **cache, const char *key, void *value) {
    Cache *new_entry = malloc(sizeof(Cache));
    new_entry->key   = strdup(key);
    new_entry->value = value;
    new_entry->next  = *cache;
    *cache           = new_entry;
}

XftFont *get_cached_font(Display *display, int screen, const char *font_name) {
    XftFont *font = get_cached_value(&font_cache, font_name);
    if (!font) {
        font = XftFontOpenName(display, screen, font_name);
        if (font) {
            add_to_cache(&font_cache, font_name, font);
        }
    }
    return font;
}

void font_cache_init(void) {
    font_cache = NULL;
}

void font_cache_cleanup(void) {
    Cache *current = font_cache;
    while (current) {
        Cache *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
    font_cache = NULL;
}
#endif

void font_init(void) {
#ifdef HAVE_XFT
    font_cache_init();
#endif
}

void font_cleanup(void) {
#ifndef HAVE_XFT
    if (dzen.font.set)
        XFreeFontSet(dzen.dpy, dzen.font.set);
    else
        XFreeFont(dzen.dpy, dzen.font.xfont);
#endif

#ifdef HAVE_XFT
    font_cache_cleanup();
#endif
}

unsigned int textnw(Fnt *font, const char *text, unsigned int len) {
#ifndef HAVE_XFT
    XRectangle r;

    if (font->set) {
        XmbTextExtents(font->set, text, len, NULL, &r);
        return r.width;
    }
    return XTextWidth(font->xfont, text, len);
#else
    XftTextExtentsUtf8(dzen.dpy, dzen.font.xftfont, (unsigned const char *)text, strlen(text), &dzen.font.extents);
    if (dzen.font.extents.height > dzen.font.height)
        dzen.font.height = dzen.font.extents.height;
    return dzen.font.extents.xOff;
#endif
}

void setfont(const char *fontstr) {
#ifndef HAVE_XFT
    char *def, **missing;
    int   i, n;

    missing = NULL;
    if (dzen.font.set)
        XFreeFontSet(dzen.dpy, dzen.font.set);

    dzen.font.set = XCreateFontSet(dzen.dpy, fontstr, &missing, &n, &def);
    if (missing)
        XFreeStringList(missing);

    if (dzen.font.set) {
        XFontSetExtents *font_extents;
        XFontStruct    **xfonts;
        char           **font_names;
        dzen.font.ascent = dzen.font.descent = 0;
        font_extents                         = XExtentsOfFontSet(dzen.font.set);
        n                                    = XFontsOfFontSet(dzen.font.set, &xfonts, &font_names);
        for (i = 0, dzen.font.ascent = 0, dzen.font.descent = 0; i < n; i++) {
            if (dzen.font.ascent < (*xfonts)->ascent)
                dzen.font.ascent = (*xfonts)->ascent;
            if (dzen.font.descent < (*xfonts)->descent)
                dzen.font.descent = (*xfonts)->descent;
            xfonts++;
        }
    } else {
        if (dzen.font.xfont)
            XFreeFont(dzen.dpy, dzen.font.xfont);
        dzen.font.xfont = NULL;
        if (!(dzen.font.xfont = XLoadQueryFont(dzen.dpy, fontstr)))
            eprint("dzen: error, cannot load font: '%s'\n", fontstr);
        dzen.font.ascent  = dzen.font.xfont->ascent;
        dzen.font.descent = dzen.font.xfont->descent;
    }
    dzen.font.height = dzen.font.ascent + dzen.font.descent;
#else
    if (dzen.font.xftfont)
        XftFontClose(dzen.dpy, dzen.font.xftfont);

    dzen.font.xftfont = get_cached_font(dzen.dpy, dzen.screen, fontstr);
    if (!dzen.font.xftfont)
        eprint("error, cannot load font: '%s'\n", fontstr);

    XftTextExtentsUtf8(dzen.dpy, dzen.font.xftfont, (unsigned const char *)fontstr, strlen(fontstr),
                       &dzen.font.extents);
    dzen.font.height = dzen.font.xftfont->ascent + dzen.font.xftfont->descent;
    dzen.font.width  = (dzen.font.extents.width) / strlen(fontstr);
#endif
}

void font_preload_single(const char *fontstr, int p) {
#ifndef HAVE_XFT
    char *def, **missing;
    int   i, n;

    missing = NULL;

    dzen.fnpl[p].set = XCreateFontSet(dzen.dpy, fontstr, &missing, &n, &def);
    if (missing)
        XFreeStringList(missing);

    if (dzen.fnpl[p].set) {
        XFontSetExtents *font_extents;
        XFontStruct    **xfonts;
        char           **font_names;
        dzen.fnpl[p].ascent = dzen.fnpl[p].descent = 0;
        font_extents                               = XExtentsOfFontSet(dzen.fnpl[p].set);
        n                                          = XFontsOfFontSet(dzen.fnpl[p].set, &xfonts, &font_names);
        for (i = 0, dzen.fnpl[p].ascent = 0, dzen.fnpl[p].descent = 0; i < n; i++) {
            if (dzen.fnpl[p].ascent < (*xfonts)->ascent)
                dzen.fnpl[p].ascent = (*xfonts)->ascent;
            if (dzen.fnpl[p].descent < (*xfonts)->descent)
                dzen.fnpl[p].descent = (*xfonts)->descent;
            xfonts++;
        }
    } else {
        if (dzen.fnpl[p].xfont)
            XFreeFont(dzen.dpy, dzen.fnpl[p].xfont);
        dzen.fnpl[p].xfont = NULL;
        if (!(dzen.fnpl[p].xfont = XLoadQueryFont(dzen.dpy, fontstr)))
            eprint("dzen: error, cannot load font: '%s'\n", fontstr);
        dzen.fnpl[p].ascent  = dzen.fnpl[p].xfont->ascent;
        dzen.fnpl[p].descent = dzen.fnpl[p].xfont->descent;
    }
    dzen.fnpl[p].height = dzen.fnpl[p].ascent + dzen.fnpl[p].descent;
#endif
}

void font_preload(char *s) {
#ifndef HAVE_XFT
    int   k   = 0;
    char *buf = strtok(s, ",");
    while (buf != NULL) {
        if (k < 64)
            font_preload_single(buf, k++);
        buf = strtok(NULL, ",");
    }
#endif
}
