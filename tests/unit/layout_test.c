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
    CHECK(layout_resolve(&input, &target, &first));
    CHECK(first.title.x == 137 && first.title.y == 73);
    CHECK(first.title.width == 211 && first.slave.width == 284);
    check_rect(first.outer, 101, 73, 284, 80);
    check_rect(first.collapsed_outer, 137, 73, 211, 20);
    check_rect(first.title_local, 36, 0, 211, 20);
    check_rect(first.slave_local, 0, 20, 284, 60);

    input.x = -1;
    input.y = -1;
    CHECK(layout_resolve(&input, &target, &first));
    CHECK(first.title.x == 689 && first.title.y == 630);

    input.x = 0;
    input.y = 590;
    CHECK(layout_resolve(&input, &target, &first));
    CHECK(first.slave.y == 570);
    check_rect(first.outer, 100, 570, 284, 80);
    check_rect(first.title_local, 0, 60, 211, 20);
    check_rect(first.slave_local, 0, 0, 284, 60);

    input.y               = 0;
    input.horizontal_menu = True;
    CHECK(layout_resolve(&input, &target, &first));
    CHECK(first.slave.y == first.title.y && first.slave.height == 20);
    check_rect(first.outer, first.slave.x, first.slave.y, 284, 20);
    check_rect(first.slave_local, 0, 0, 284, 20);
    CHECK(first.title_local.x == 0);
    layout_menu_child(&first, 2, 3, &child);
    CHECK(child.x == 188 && child.width == 96);

    CHECK(layout_resolve(&input, &target, &second));
    CHECK(layout_equal(&first, &second));

    input.horizontal_menu = False;
    input.max_lines       = 0;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, first.title.x, first.title.y, 211, 20);
    check_rect(first.collapsed_outer, first.title.x, first.title.y, 211, 20);
    check_rect(first.title_local, 0, 0, 211, 20);

    input               = request();
    input.x             = 0;
    input.y             = 0;
    input.border.top    = 5;
    input.border.right  = 7;
    input.border.bottom = 9;
    input.border.left   = 11;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, 100, 50, 800, 94);
    check_rect(first.title, 111, 55, 782, 20);
    check_rect(first.slave, 111, 75, 782, 60);
    check_rect(first.surface, 111, 55, 782, 80);
    check_rect(first.title_local, 0, 0, 782, 20);
    check_rect(first.slave_local, 0, 20, 782, 60);
    check_rect(first.collapsed_outer, 100, 50, 800, 34);

    input.title_width          = 200;
    input.slave_width          = 300;
    input.title_width_explicit = True;
    input.slave_width_explicit = True;
    input.x                    = 250;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, 289, 50, 318, 94);
    check_rect(first.title, 350, 55, 200, 20);
    check_rect(first.slave, 300, 75, 300, 60);

    input.x           = 700;
    input.slave_width = 900;
    input.title_width = 900;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, 100, 50, 918, 94);
    check_rect(first.title, 111, 55, 900, 20);

    input                 = request();
    input.horizontal_menu = True;
    input.border.top      = 2;
    input.border.right    = 3;
    input.border.bottom   = 4;
    input.border.left     = 5;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, 100, 50, 800, 26);
    check_rect(first.slave, 105, 52, 792, 20);

    input                      = request();
    input.x                    = 250;
    input.title_width          = 200;
    input.slave_width          = 300;
    input.title_width_explicit = True;
    input.slave_width_explicit = True;
    input.padding.top          = 3;
    input.padding.right        = 4;
    input.padding.bottom       = 5;
    input.padding.left         = 6;
    input.border.top           = 2;
    input.border.right         = 7;
    input.border.bottom        = 11;
    input.border.left          = 13;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.title, 350, 55, 200, 20);
    check_rect(first.slave, 300, 75, 300, 60);
    check_rect(first.surface, 294, 52, 310, 88);
    check_rect(first.outer, 281, 50, 330, 101);
    check_rect(first.collapsed_surface, 344, 52, 210, 28);
    check_rect(first.collapsed_outer, 331, 50, 230, 41);
    check_rect(first.title_local, 56, 3, 200, 20);
    check_rect(first.slave_local, 6, 23, 300, 60);

    input               = request();
    input.padding.left  = 5;
    input.padding.right = 7;
    input.border.left   = 11;
    input.border.right  = 13;
    CHECK(layout_resolve(&input, &target, &first));
    check_rect(first.outer, 100, 50, 800, 80);
    check_rect(first.surface, 111, 50, 776, 80);
    check_rect(first.title, 116, 50, 764, 20);

    input                      = request();
    input.title_width          = 65535;
    input.slave_width          = 65535;
    input.title_width_explicit = True;
    input.slave_width_explicit = True;
    input.border.left          = 1;
    CHECK(!layout_resolve(&input, &target, &first));

    input                      = request();
    input.title_width          = 65535;
    input.title_width_explicit = True;
    input.padding.left         = 1;
    CHECK(!layout_resolve(&input, &target, &first));

    input             = request();
    input.line_height = 40000;
    input.max_lines   = 2;
    CHECK(!layout_resolve(&input, &target, &first));

    puts("layout tests passed");
    return EXIT_SUCCESS;
}
