#include "border.h"
#include "test_common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_widths(const BorderSpec *spec, unsigned int top, unsigned int right, unsigned int bottom,
                         unsigned int left) {
    CHECK(spec->widths.top == top);
    CHECK(spec->widths.right == right);
    CHECK(spec->widths.bottom == bottom);
    CHECK(spec->widths.left == left);
}

static void check_rejected_unchanged(BorderSpec *spec, const char *text) {
    CHECK(!border_spec_parse(spec, text));
    check_widths(spec, 3, 4, 3, 4);
    CHECK(spec->color_explicit);
    CHECK(strcmp(spec->color, "blue") == 0);
}

static void check_insets(BoxInsets insets, unsigned int top, unsigned int right, unsigned int bottom,
                         unsigned int left) {
    CHECK(insets.top == top);
    CHECK(insets.right == right);
    CHECK(insets.bottom == bottom);
    CHECK(insets.left == left);
}

int main(void) {
    BorderSpec spec;
    BoxInsets  insets = { 3, 4, 3, 4 };

    CHECK(box_insets_parse(&insets, "10"));
    check_insets(insets, 10, 10, 10, 10);
    CHECK(box_insets_parse(&insets, " 11 , 14 "));
    check_insets(insets, 11, 14, 11, 14);
    CHECK(box_insets_parse(&insets, "1,2,3,4"));
    check_insets(insets, 1, 2, 3, 4);
    CHECK(box_insets_parse(&insets, "0"));
    CHECK(!box_insets_visible(&insets));
    CHECK(box_insets_parse(&insets, "3,4"));
    CHECK(!box_insets_parse(&insets, NULL));
    CHECK(!box_insets_parse(&insets, ""));
    CHECK(!box_insets_parse(&insets, "1,2,3"));
    CHECK(!box_insets_parse(&insets, "1,2,3,4,5"));
    CHECK(!box_insets_parse(&insets, "-1"));
    CHECK(!box_insets_parse(&insets, "4294967296"));
    check_insets(insets, 3, 4, 3, 4);

    border_spec_init(&spec);
    CHECK(border_spec_parse(&spec, "10"));
    check_widths(&spec, 10, 10, 10, 10);
    CHECK(!spec.color_explicit && spec.color == NULL);

    CHECK(border_spec_parse(&spec, " 11 , 14 "));
    check_widths(&spec, 11, 14, 11, 14);
    CHECK(!spec.color_explicit);

    CHECK(border_spec_parse(&spec, "10,red"));
    check_widths(&spec, 10, 10, 10, 10);
    CHECK(spec.color_explicit && strcmp(spec.color, "red") == 0);

    CHECK(border_spec_parse(&spec, "10, 8, #123456"));
    check_widths(&spec, 10, 8, 10, 8);
    CHECK(spec.color_explicit && strcmp(spec.color, "#123456") == 0);

    CHECK(border_spec_parse(&spec, "1,2,3,4, rgb:1/2/3"));
    check_widths(&spec, 1, 2, 3, 4);
    CHECK(spec.color_explicit && strcmp(spec.color, "rgb:1/2/3") == 0);

    CHECK(border_spec_parse(&spec, "0,orange"));
    CHECK(!border_spec_visible(&spec));
    CHECK(spec.color_explicit);

    CHECK(border_spec_parse(&spec, "3,4,blue"));
    check_rejected_unchanged(&spec, NULL);
    check_rejected_unchanged(&spec, "");
    check_rejected_unchanged(&spec, "1,");
    check_rejected_unchanged(&spec, ",1");
    check_rejected_unchanged(&spec, "1,,red");
    check_rejected_unchanged(&spec, "-1");
    check_rejected_unchanged(&spec, "+1");
    check_rejected_unchanged(&spec, "1 2");
    check_rejected_unchanged(&spec, "1,2,3");
    check_rejected_unchanged(&spec, "1,2,3,4,red,extra");
    check_rejected_unchanged(&spec, "4294967296");
    check_rejected_unchanged(&spec, "999999999999999999999999999999999999");

    CHECK(border_spec_parse(&spec, "7"));
    check_widths(&spec, 7, 7, 7, 7);
    CHECK(!spec.color_explicit && spec.color == NULL);

    CHECK(border_spec_parse(&spec, "9,#abcdef"));
    CHECK(spec.color_explicit && strcmp(spec.color, "#abcdef") == 0);
    CHECK(border_spec_parse(&spec, "5,6"));
    check_widths(&spec, 5, 6, 5, 6);
    CHECK(!spec.color_explicit && spec.color == NULL);

    CHECK(border_spec_parse(&spec, "3,4,blue"));
    check_rejected_unchanged(&spec, "broken");

    border_spec_destroy(&spec);
    puts("border and inset parser tests passed");
    return EXIT_SUCCESS;
}
