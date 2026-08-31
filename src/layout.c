#include "layout.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define X11_MAX_DIMENSION 65535

typedef struct {
    int64_t x;
    int64_t y;
    int64_t width;
    int64_t height;
} WideRect;

static int64_t positive_size(int64_t value) {
    return value > 0 ? value : 1;
}

static int64_t anchored_position(int requested, int64_t origin, int64_t extent, int64_t object_extent) {
    if (requested < 0)
        return origin + extent - object_extent + requested + 1;
    return origin + requested;
}

static int64_t clamp_position(int64_t position, int64_t origin, int64_t extent, int64_t object_extent) {
    if (object_extent >= extent)
        return origin;
    if (position < origin)
        return origin;
    if (position + object_extent > origin + extent)
        return origin + extent - object_extent;
    return position;
}

static WideRect rect_union(const WideRect *left, const WideRect *right) {
    WideRect result;
    int64_t  right_edge  = left->x + left->width > right->x + right->width ? left->x + left->width
                                                                           : right->x + right->width;
    int64_t  bottom_edge = left->y + left->height > right->y + right->height ? left->y + left->height
                                                                             : right->y + right->height;

    result.x      = left->x < right->x ? left->x : right->x;
    result.y      = left->y < right->y ? left->y : right->y;
    result.width  = right_edge - result.x;
    result.height = bottom_edge - result.y;
    return result;
}

static WideRect bordered(const WideRect *content, const BorderInsets *border) {
    WideRect result;

    result.x      = content->x - border->left;
    result.y      = content->y - border->top;
    result.width  = content->width + (int64_t)border->left + border->right;
    result.height = content->height + (int64_t)border->top + border->bottom;
    return result;
}

static Bool rect_is_safe(const WideRect *rect) {
    return rect->x >= INT_MIN && rect->x <= INT_MAX && rect->y >= INT_MIN && rect->y <= INT_MAX && rect->width > 0 &&
           rect->width <= X11_MAX_DIMENSION && rect->height > 0 && rect->height <= X11_MAX_DIMENSION;
}

static void narrow_rect(const WideRect *wide, LayoutRect *result) {
    result->x      = (int)wide->x;
    result->y      = (int)wide->y;
    result->width  = (int)wide->width;
    result->height = (int)wide->height;
}

Bool layout_resolve(const LayoutRequest *request, const XRectangle *target, ResolvedLayout *result) {
    ResolvedLayout resolved;
    WideRect       title;
    WideRect       slave;
    WideRect       content_outer;
    WideRect       outer;
    WideRect       collapsed_outer;
    int64_t        target_x      = target->x;
    int64_t        target_y      = target->y;
    int64_t        target_width  = target->width;
    int64_t        target_height = target->height;
    int64_t        implicit_width;
    int64_t        title_width;
    int64_t        slave_width;
    int64_t        translation_x;
    int64_t        translation_y;

    memset(&resolved, 0, sizeof(resolved));
    implicit_width = positive_size(target_width - (int64_t)request->border.left - request->border.right);

    if (request->title_width_explicit)
        title_width = positive_size(request->title_width);
    else if (request->slave_width_explicit)
        title_width = positive_size(request->slave_width);
    else
        title_width = implicit_width;

    if (request->slave_width_explicit)
        slave_width = positive_size(request->slave_width);
    else
        slave_width = implicit_width;

    title.x      = anchored_position(request->x, target_x, target_width, title_width);
    title.y      = anchored_position(request->y, target_y, target_height, request->line_height);
    title.x      = clamp_position(title.x, target_x, target_width, title_width);
    title.y      = clamp_position(title.y, target_y, target_height, request->line_height);
    title.width  = title_width;
    title.height = request->line_height;

    slave.x = title.x;
    if (slave_width != title_width) {
        slave.x = title.x + (title_width - slave_width) / 2;
        slave.x = clamp_position(slave.x, target_x, target_width, slave_width);
    }
    if (request->horizontal_menu) {
        slave.y = title.y;
    } else {
        slave.y = title.y + request->line_height;
        if (slave.y + (int64_t)request->max_lines * request->line_height > target_y + target_height)
            slave.y = title.y - (int64_t)request->max_lines * request->line_height;
        if (slave.y < target_y)
            slave.y = target_y;
    }
    slave.width  = slave_width;
    slave.height = request->horizontal_menu ? request->line_height : (int64_t)request->max_lines * request->line_height;

    if (!request->max_lines)
        content_outer = title;
    else if (request->horizontal_menu)
        content_outer = slave;
    else
        content_outer = rect_union(&title, &slave);
    outer           = bordered(&content_outer, &request->border);
    collapsed_outer = bordered(&title, &request->border);

    translation_x = clamp_position(outer.x, target_x, target_width, outer.width) - outer.x;
    translation_y = clamp_position(outer.y, target_y, target_height, outer.height) - outer.y;
    title.x += translation_x;
    title.y += translation_y;
    slave.x += translation_x;
    slave.y += translation_y;
    outer.x += translation_x;
    outer.y += translation_y;
    collapsed_outer.x += translation_x;
    collapsed_outer.y += translation_y;

    if (!rect_is_safe(&title) || !rect_is_safe(&outer) || !rect_is_safe(&collapsed_outer) ||
        (request->max_lines && !rect_is_safe(&slave)))
        return False;

    narrow_rect(&title, &resolved.title);
    narrow_rect(&slave, &resolved.slave);
    narrow_rect(&outer, &resolved.outer);
    narrow_rect(&collapsed_outer, &resolved.collapsed_outer);
    resolved.title_local.x      = (int)(title.x - outer.x);
    resolved.title_local.y      = (int)(title.y - outer.y);
    resolved.title_local.width  = resolved.title.width;
    resolved.title_local.height = resolved.title.height;
    resolved.slave_local.x      = (int)(slave.x - outer.x);
    resolved.slave_local.y      = (int)(slave.y - outer.y);
    resolved.slave_local.width  = resolved.slave.width;
    resolved.slave_local.height = resolved.slave.height;
    resolved.title_right        = resolved.title.x + resolved.title.width;
    resolved.border             = request->border;

    if (request->horizontal_menu && request->max_lines > 0) {
        resolved.menu_entry_width = resolved.slave.width / request->max_lines;
        resolved.menu_last_width  = resolved.menu_entry_width + resolved.slave.width % request->max_lines;
        if (resolved.menu_entry_width == 0)
            return False;
    }

    *result = resolved;
    return True;
}

void layout_menu_child(const ResolvedLayout *layout, int index, int count, LayoutRect *result) {
    result->x      = index * layout->menu_entry_width;
    result->y      = 0;
    result->width  = index == count - 1 ? layout->menu_last_width : layout->menu_entry_width;
    result->height = layout->slave.height;
}

Bool layout_equal(const ResolvedLayout *left, const ResolvedLayout *right) {
    return memcmp(left, right, sizeof(*left)) == 0;
}
