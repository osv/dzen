#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "dzen.h"
#include "kvstore.h"

static KeyValueStore *color_store;
static KeyValueStore *icon_store;

/*
 * New constructor for color items.  This is used by kvstore_find_or_create
 * whenever a key is not found in the store.  We return a pointer to a long
 * initialized to -1, meaning "not yet allocated via XAllocNamedColor".
 */
static void *color_create_item(void) {
    long *pixel_ptr = malloc(sizeof(long));
    if (pixel_ptr)
        *pixel_ptr = -1;
    return pixel_ptr;
}

long get_color(const char *colstr) {
    if (!colstr || !*colstr)
        return -1;

    long *pixel_ptr = (long *)kvstore_find_or_create(color_store, colstr);
    if (!pixel_ptr)
        return -1; /* constructor not set or store is broken? */

    if (*pixel_ptr != -1) {
        /* Already allocated and cached. */
        return *pixel_ptr;
    }

    /* Not yet allocated in X; do it now and store the pixel. */
    Colormap cmap = DefaultColormap(dzen.dpy, dzen.screen);
    XColor   color;
    if (!XAllocNamedColor(dzen.dpy, cmap, colstr, &color, &color)) {
        return -1; /* invalid color? */
    }

    *pixel_ptr = color.pixel;
    return color.pixel;
}

static void icon_destroy_item(void *value) {
    Icon *icon = (Icon *)value;

    if (!icon)
        return;

    if (icon->pm) {
        XFreePixmap(dzen.dpy, icon->pm);
    }

    /* If mask_pm was created (transparency), free it too. */
    if (icon->mask_pm) {
        XFreePixmap(dzen.dpy, icon->mask_pm);
    }

#ifdef HAVE_XPM
    /* Pseudocolor note: free xpma color cells if needed */
    if (icon->xpma.npixels > 0) {
        XFreeColors(dzen.dpy, icon->xpma.colormap, icon->xpma.pixels, icon->xpma.npixels, 0);
    }
    XpmFreeAttributes(&icon->xpma);
#endif

    free(icon);
}

static char cached_cwd[PATH_MAX] = { 0 };

static const char *get_cached_cwd() {
    if (cached_cwd[0] == '\0') {
        if (!getcwd(cached_cwd, sizeof(cached_cwd))) {
            /* If getcwd fails, set cached_cwd to an empty string */
            cached_cwd[0] = '\0';
        }
    }
    return cached_cwd;
}

static char *expand_path(const char *path) {
    if (!path)
        return NULL;

    char *expanded = NULL;
    if (path[0] == '~') {
        /* Expand ~ to home directory */
        const char *home = getenv("HOME");
        if (!home) {
            errno = ENOENT;
            return NULL;
        }
        /* Allocate space for home + rest of path + null terminator */
        expanded = malloc(strlen(home) + strlen(path));
        if (!expanded)
            return NULL;
        sprintf(expanded, "%s%s", home, path + 1);
    } else if (path[0] != '/') {
        /* Relative path: use cached CWD */
        const char *cwd = get_cached_cwd();
        if (!cwd || cwd[0] == '\0') {
            return NULL;
        }
        expanded = malloc(strlen(cwd) + strlen(path) + 2);
        if (!expanded)
            return NULL;
        sprintf(expanded, "%s/%s", cwd, path);
    } else {
        /* Absolute path: simply duplicate it */
        expanded = strdup(path);
    }
    return expanded;
}

static int icon_load_xpm(const char *path, Icon *icon) {
#ifdef HAVE_XPM
    /* Zero-initialize so XpmFreeAttributes won't break if something fails early */
    memset(&icon->xpma, 0, sizeof(icon->xpma));
    icon->is_xbm  = False;
    icon->pm      = None;
    icon->mask_pm = None;

    icon->xpma.colormap  = DefaultColormap(dzen.dpy, dzen.screen);
    icon->xpma.depth     = DefaultDepth(dzen.dpy, dzen.screen);
    icon->xpma.visual    = DefaultVisual(dzen.dpy, dzen.screen);
    icon->xpma.valuemask = XpmColormap | XpmDepth | XpmVisual;

    /*
     * By passing &icon->mask_pm, if the XPM has "None" (transparency),
     * we'll get back a valid 1-bit mask, or None if the XPM is fully opaque.
     */
    if (XpmReadFileToPixmap(dzen.dpy, dzen.title_win.win, path, &icon->pm, /* OUT: the colored Pixmap */
                            &icon->mask_pm, /* OUT: the mask Pixmap (1-bit) */
                            &icon->xpma) == XpmSuccess) {
        icon->w = icon->xpma.width;
        icon->h = icon->xpma.height;
        return 0; // success
    }
#endif
    return 1;
}

static int icon_load_xbm(const char *path, Icon *icon) {
    Pixmap       bm = None;
    unsigned int bm_w, bm_h;
    int          bm_xh, bm_yh;

    if (XReadBitmapFile(dzen.dpy, dzen.title_win.win, path, &bm_w, &bm_h, &bm, &bm_xh, &bm_yh) == BitmapSuccess) {
        icon->mask_pm = None;
        icon->is_xbm  = True;
        icon->pm      = bm;
        icon->w       = bm_w;
        icon->h       = bm_h;
        return 0; /* success */
    }
    return 1; /* failure */
}

Icon *get_icon(const char *path) {
    if (!path || !*path)
        return NULL;

    /* Expand the path to an absolute path with ~ and relative paths resolved */
    char *expanded_path = expand_path(path);
    if (!expanded_path) {
        return NULL;
    }

    /* Check if the icon is already cached. */
    Icon *icon = (Icon *)kvstore_get(icon_store, expanded_path);
    if (icon) {
        free(expanded_path);
        return icon;
    }

    /* Otherwise, we must load a new icon. */
    icon = (Icon *)malloc(sizeof(Icon));
    if (!icon) {
        free(expanded_path);
        return NULL; /* out of memory */
    }
    icon->pm = None;
    icon->w  = 0;
    icon->h  = 0;

    /* 1) Try XPM if available. If that fails, 2) fall back to XBM. */
    if (icon_load_xpm(expanded_path, icon) != 0) {
        /* Fallback to XBM. */
        if (icon_load_xbm(expanded_path, icon) != 0) {
            /* Both attempts failed. */
            free(icon);
            free(expanded_path);
            return NULL;
        }
    }

    /* Store it in the kvstore for future use. */
    kvstore_set(icon_store, expanded_path, icon);
    free(expanded_path);
    return icon;
}

void init_all_caches() {
    icon_store  = kvstore_create(icon_destroy_item, NULL);
    color_store = kvstore_create(NULL, color_create_item);
}

void free_all_caches() {
    kvstore_destroy(icon_store);
    kvstore_destroy(color_store);
}
