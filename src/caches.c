#include "dzen.h"

#include "kvstore.h"

KeyValueStore* color_store;

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

void
init_all_caches() {
    color_store = kvstore_create(NULL, color_create_item);
}

void
free_all_caches() {
    kvstore_destroy(color_store);
}
