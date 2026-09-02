#ifndef DZEN_LAYOUT_H
#define DZEN_LAYOUT_H

#include <X11/Xlib.h>

#include "border.h"

typedef struct {
    int          x; /* Horizontal content offset; negative anchors to target's right edge. */
    int          y; /* Vertical content offset; negative anchors to target's bottom edge. */
    int          title_width; /* Requested title content width, excluding padding and border. */
    int          slave_width; /* Slave content width, with the same exclusions. */
    int          line_height; /* Content height of one title/menu line. */
    int          max_lines; /* Slave line count; zero means title-only. */
    int          expand; /* Dynamic title expansion mode. */
    Bool         title_width_explicit; /* Otherwise derive the title width from slave/target geometry. */
    Bool         slave_width_explicit; /* Otherwise derive the slave width from target geometry. */
    Bool         horizontal_menu; /* Slave entries share one row instead of stacking vertically. */
    BorderInsets border; /* Outermost box-model insets. */
    BoxInsets    padding; /* Insets between the content union and border. */
} LayoutRequest;

typedef struct {
    int x; /* Horizontal origin in the owner's coordinate space. */
    int y; /* Vertical origin in the owner's coordinate space. */
    int width; /* Positive horizontal drawable extent. */
    int height; /* Positive vertical drawable extent. */
} LayoutRect;

typedef struct {
    LayoutRect   title; /* Absolute content geometry in root coordinates. */
    LayoutRect   slave; /* Absolute slave content geometry. */
    LayoutRect   surface; /* Visible content union plus padding. */
    LayoutRect   collapsed_surface; /* Title-only surface plus padding. */
    LayoutRect   outer; /* Surface plus border; the WM-facing window. */
    LayoutRect   collapsed_outer; /* Collapsed surface plus border. */
    LayoutRect   title_local; /* Content-child geometry relative to surface. */
    LayoutRect   slave_local; /* Slave-child geometry relative to surface. */
    BorderInsets border; /* Resolved copies used by state-specific relayout. */
    BoxInsets    padding; /* Resolved padding copy. */
    int          title_right; /* Absolute right edge retained for dynamic expansion. */
    int          menu_entry_width; /* Equal horizontal-menu entry width. */
    int          menu_last_width; /* Last entry absorbs the division remainder. */
} ResolvedLayout;

Bool layout_resolve(const LayoutRequest *request, const XRectangle *target, ResolvedLayout *result);
void layout_menu_child(const ResolvedLayout *layout, int index, int count, LayoutRect *result);
Bool layout_equal(const ResolvedLayout *left, const ResolvedLayout *right);

#endif
