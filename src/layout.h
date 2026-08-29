#ifndef DZEN_LAYOUT_H
#define DZEN_LAYOUT_H

#include <X11/Xlib.h>

typedef struct {
    int  x;
    int  y;
    int  title_width;
    int  slave_width;
    int  line_height;
    int  max_lines;
    int  expand;
    Bool title_width_explicit;
    Bool slave_width_explicit;
    Bool horizontal_menu;
} LayoutRequest;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} LayoutRect;

typedef struct {
    LayoutRect title;
    LayoutRect slave;
    int        title_right;
    int        menu_entry_width;
    int        menu_last_width;
} ResolvedLayout;

void layout_resolve(const LayoutRequest *request, const XRectangle *target, ResolvedLayout *result);
void layout_menu_child(const ResolvedLayout *layout, int index, int count, LayoutRect *result);
Bool layout_equal(const ResolvedLayout *left, const ResolvedLayout *right);

#endif
