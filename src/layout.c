#include "layout.h"

#include <string.h>

static int positive_size(int value) {
    return value > 0 ? value : 1;
}

static int anchored_position(int requested, int origin, int extent, int object_extent) {
    if (requested < 0)
        return origin + extent - object_extent + requested + 1;
    return origin + requested;
}

static int clamp_position(int position, int origin, int extent, int object_extent) {
    if (object_extent >= extent)
        return origin;
    if (position < origin)
        return origin;
    if (position + object_extent > origin + extent)
        return origin + extent - object_extent;
    return position;
}

static LayoutRect rect_union(const LayoutRect *left, const LayoutRect *right) {
    LayoutRect result;
    int right_edge  = left->x + left->width > right->x + right->width ? left->x + left->width : right->x + right->width;
    int bottom_edge = left->y + left->height > right->y + right->height ? left->y + left->height
                                                                        : right->y + right->height;

    result.x      = left->x < right->x ? left->x : right->x;
    result.y      = left->y < right->y ? left->y : right->y;
    result.width  = right_edge - result.x;
    result.height = bottom_edge - result.y;
    return result;
}

void layout_resolve(const LayoutRequest *request, const XRectangle *target, ResolvedLayout *result) {
    int title_width;
    int slave_width;
    int title_x;
    int title_y;
    int slave_x;
    int slave_y;

    memset(result, 0, sizeof(*result));

    if (request->title_width_explicit)
        title_width = positive_size(request->title_width);
    else if (request->slave_width_explicit)
        title_width = positive_size(request->slave_width);
    else
        title_width = positive_size(target->width);

    if (request->slave_width_explicit)
        slave_width = positive_size(request->slave_width);
    else
        slave_width = positive_size(target->width);

    title_x = anchored_position(request->x, target->x, target->width, title_width);
    title_y = anchored_position(request->y, target->y, target->height, request->line_height);
    title_x = clamp_position(title_x, target->x, target->width, title_width);
    title_y = clamp_position(title_y, target->y, target->height, request->line_height);

    slave_x = title_x;
    if (slave_width != title_width) {
        slave_x = title_x + (title_width - slave_width) / 2;
        slave_x = clamp_position(slave_x, target->x, target->width, slave_width);
    }

    if (request->horizontal_menu) {
        slave_y = title_y;
    } else {
        slave_y = title_y + request->line_height;
        if (slave_y + request->max_lines * request->line_height > target->y + target->height)
            slave_y = title_y - request->max_lines * request->line_height;
        if (slave_y < target->y)
            slave_y = target->y;
    }

    result->title.x      = title_x;
    result->title.y      = title_y;
    result->title.width  = title_width;
    result->title.height = request->line_height;
    result->title_right  = title_x + title_width;

    result->slave.x      = slave_x;
    result->slave.y      = slave_y;
    result->slave.width  = slave_width;
    result->slave.height = request->horizontal_menu ? request->line_height : request->max_lines * request->line_height;

    result->collapsed_outer = result->title;
    if (!request->max_lines)
        result->outer = result->title;
    else if (request->horizontal_menu)
        result->outer = result->slave;
    else
        result->outer = rect_union(&result->title, &result->slave);

    result->title_local.x      = result->title.x - result->outer.x;
    result->title_local.y      = result->title.y - result->outer.y;
    result->title_local.width  = result->title.width;
    result->title_local.height = result->title.height;
    result->slave_local.x      = result->slave.x - result->outer.x;
    result->slave_local.y      = result->slave.y - result->outer.y;
    result->slave_local.width  = result->slave.width;
    result->slave_local.height = result->slave.height;

    if (request->horizontal_menu && request->max_lines > 0) {
        result->menu_entry_width = slave_width / request->max_lines;
        result->menu_last_width  = result->menu_entry_width + slave_width % request->max_lines;
    }
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
