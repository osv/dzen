#include "test_common.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_valid(const char *text, unsigned int expected_thickness, const char *expected_color) {
    unsigned int thickness = 99;
    char         color[64] = "unchanged";

    CHECK(get_decor_vals(text, &thickness, color, sizeof(color)));
    CHECK(thickness == expected_thickness);
    CHECK(strcmp(color, expected_color) == 0);
}

static void check_invalid(const char *text) {
    unsigned int thickness = 99;
    char         color[64] = "unchanged";

    CHECK(!get_decor_vals(text, &thickness, color, sizeof(color)));
    CHECK(thickness == 0);
    CHECK(color[0] == '\0');
}

int main(void) {
    char tiny[2];

    check_valid("", 0, "");
    check_valid("1", 1, "");
    check_valid("red", 0, "red");
    check_valid("rgb:1/2/3", 0, "rgb:1/2/3");
    check_valid("2,red", 2, "red");
    check_valid("off", 0, "off");

    check_invalid(NULL);
    check_invalid(" ");
    check_invalid(" 12");
    check_invalid("12 ");
    check_invalid("#abcdef ");
    check_invalid("3,#abcdef ");
    check_invalid("3, #abcdef");
    check_invalid("3 ,#abcdef");
    check_invalid("red blue");
    check_invalid("off ");
    check_invalid("1\t,#abcdef");
    check_invalid("0");
    check_invalid("-1");
    check_invalid("+1");
    check_invalid("1,");
    check_invalid(",red");
    check_invalid("1,,red");
    check_invalid("1,red,blue");
    check_invalid("4294967296");
    check_invalid("999999999999999999999999999999999999");

    CHECK(!get_decor_vals("red", &(unsigned int){ 0 }, tiny, sizeof(tiny)));
    CHECK(!get_decor_vals("1", NULL, tiny, sizeof(tiny)));
    CHECK(!get_decor_vals("1", &(unsigned int){ 0 }, NULL, sizeof(tiny)));
    CHECK(!get_decor_vals("1", &(unsigned int){ 0 }, tiny, 0));

    puts("decoration parser tests passed");
    return EXIT_SUCCESS;
}
