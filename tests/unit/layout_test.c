#include "layout.h"
#include "test_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LayoutRequest request(void) {
    LayoutRequest result;

    memset(&result, 0, sizeof(result));
    result.line_height = 20;
    result.max_lines   = 3;
    return result;
}

static void check_rect(LayoutRect rect, int x, int y, int width, int height) {
    CHECK(rect.x == x);
    CHECK(rect.y == y);
    CHECK(rect.width == width);
    CHECK(rect.height == height);
}

int main(void) {
    XRectangle     target = { 100, 50, 800, 600 };
    LayoutRequest  input  = request();
    ResolvedLayout first;
    ResolvedLayout second;
    LayoutRect     child;

    input.x                    = 37;
    input.y                    = 23;
    input.title_width          = 211;
    input.slave_width          = 284;
    input.title_width_explicit = True;
    input.slave_width_explicit = True;
    layout_resolve(&input, &target, &first);
    CHECK(first.title.x == 137 && first.title.y == 73);
    CHECK(first.title.width == 211 && first.slave.width == 284);
    check_rect(first.outer, 101, 73, 284, 80);
    check_rect(first.collapsed_outer, 137, 73, 211, 20);
    check_rect(first.title_local, 36, 0, 211, 20);
    check_rect(first.slave_local, 0, 20, 284, 60);

    input.x = -1;
    input.y = -1;
    layout_resolve(&input, &target, &first);
    CHECK(first.title.x == 689 && first.title.y == 630);

    input.x = 0;
    input.y = 590;
    layout_resolve(&input, &target, &first);
    CHECK(first.slave.y == 570);
    check_rect(first.outer, 100, 570, 284, 80);
    check_rect(first.title_local, 0, 60, 211, 20);
    check_rect(first.slave_local, 0, 0, 284, 60);

    input.y               = 0;
    input.horizontal_menu = True;
    layout_resolve(&input, &target, &first);
    CHECK(first.slave.y == first.title.y && first.slave.height == 20);
    check_rect(first.outer, first.slave.x, first.slave.y, 284, 20);
    check_rect(first.slave_local, 0, 0, 284, 20);
    CHECK(first.title_local.x == 0);
    layout_menu_child(&first, 2, 3, &child);
    CHECK(child.x == 188 && child.width == 96);

    layout_resolve(&input, &target, &second);
    CHECK(layout_equal(&first, &second));

    input.horizontal_menu = False;
    input.max_lines       = 0;
    layout_resolve(&input, &target, &first);
    check_rect(first.outer, first.title.x, first.title.y, 211, 20);
    check_rect(first.collapsed_outer, first.title.x, first.title.y, 211, 20);
    check_rect(first.title_local, 0, 0, 211, 20);

    puts("layout tests passed");
    return EXIT_SUCCESS;
}
