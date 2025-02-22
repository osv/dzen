#include "dzen.h"
#include "kvstore.h"

static KeyValueStore* color_store;
static KeyValueStore* icon_store;

/*
 * New constructor for color items.  This is used by kvstore_find_or_create
 * whenever a key is not found in the store.  We return a pointer to a long
 * initialized to -1, meaning "not yet allocated via XAllocNamedColor".
 */
static void*
color_create_item(void) {
    long* pixel_ptr = malloc(sizeof(long));
    if (pixel_ptr)
        *pixel_ptr = -1;
    return pixel_ptr;
}

long
get_color(const char* colstr) {
    if (!colstr || !*colstr)
        return -1;

    long* pixel_ptr = (long*) kvstore_find_or_create(color_store, colstr);
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

static void
icon_destroy_item(void* value) {
    Icon* icon = (Icon*)value;

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
        XFreeColors(dzen.dpy, icon->xpma.colormap,
                    icon->xpma.pixels, icon->xpma.npixels, 0);
    }
    XpmFreeAttributes(&icon->xpma);
#endif

    free(icon);
}

static int
icon_load_xpm(const char* path, Icon* icon) {
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
    if (XpmReadFileToPixmap(dzen.dpy, dzen.title_win.win,
                            path,
                            &icon->pm,      /* OUT: the colored Pixmap */
                            &icon->mask_pm, /* OUT: the mask Pixmap (1-bit) */
                            &icon->xpma) == XpmSuccess) {
        icon->w = icon->xpma.width;
        icon->h = icon->xpma.height;
        return 0; // success
    }
#endif
    return 1;
}

static int
icon_load_xbm(const char* path, Icon* icon) {
    Pixmap       bm = None;
    unsigned int bm_w, bm_h;
    int          bm_xh, bm_yh;

    if (XReadBitmapFile(dzen.dpy, dzen.title_win.win, path,
                        &bm_w, &bm_h, &bm, &bm_xh, &bm_yh) == BitmapSuccess) {
        icon->mask_pm = None;
        icon->is_xbm = True;
        icon->pm = bm;
        icon->w  = bm_w;
        icon->h  = bm_h;
        return 0; /* success */
    }
    return 1; /* failure */
}

/*
 * Main function that gets an icon from the store or loads it into the store.
 */
Icon*
get_icon(const char* path) {
    if (!path || !*path)
        return NULL;

    /* Check if the icon is already cached. */
    Icon* icon = (Icon*) kvstore_get(icon_store, path);
    if (icon) {
        /* Already in the cache */
        return icon;
    }

    /* Otherwise, we must load a new icon. */
    icon = (Icon*) malloc(sizeof(Icon));
    if (!icon) {
        return NULL; /* out of memory */
    }
    icon->pm = None;
    icon->w  = 0;
    icon->h  = 0;

    /* 1) Try XPM if available. If that fails, 2) fall back to XBM. */
    if (icon_load_xpm(path, icon) != 0) {
        /* Fallback to XBM. */
        if (icon_load_xbm(path, icon) != 0) {
            /* Both attempts failed. */
            free(icon);
            return NULL;
        }
    }

    /* Store it in the kvstore for future use. */
    kvstore_set(icon_store, path, icon);
    return icon;
}

void
init_all_caches() {
    icon_store  = kvstore_create(icon_destroy_item, NULL);
    color_store = kvstore_create(NULL, color_create_item);
}

void
free_all_caches() {
    kvstore_destroy(icon_store);
    kvstore_destroy(color_store);
}
