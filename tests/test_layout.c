#include "../src/layout.h"
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

    input.x = -1;
    input.y = -1;
    layout_resolve(&input, &target, &first);
    CHECK(first.title.x == 689 && first.title.y == 630);

    input.x = 0;
    input.y = 590;
    layout_resolve(&input, &target, &first);
    CHECK(first.slave.y == 570);

    input.y               = 0;
    input.horizontal_menu = True;
    layout_resolve(&input, &target, &first);
    CHECK(first.slave.y == first.title.y && first.slave.height == 20);
    layout_menu_child(&first, 2, 3, &child);
    CHECK(child.x == 188 && child.width == 96);

    layout_resolve(&input, &target, &second);
    CHECK(layout_equal(&first, &second));

    puts("layout tests passed");
    return EXIT_SUCCESS;
}
