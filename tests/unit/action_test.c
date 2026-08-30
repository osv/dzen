#include "action.h"
#include "dzen.h"
#include "test_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Dzen        dzen = { 0 };
static int  draw_body_calls;
static int  spawn_calls;
static char spawned_command[64];

void       *emalloc(unsigned int size) {
    void *result = malloc(size);

    CHECK(result != NULL);
    return result;
}

char *estrdup(const char *text) {
    char *result = strdup(text);

    CHECK(result != NULL);
    return result;
}

void eprint(const char *format, ...) {
    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    exit(EXIT_FAILURE);
}

void spawn(const char *command) {
    spawn_calls++;
    snprintf(spawned_command, sizeof(spawned_command), "%s", command);
}

void x_draw_body(void) {
    draw_body_calls++;
}

void parse_line_text(const char *text, TextBuffer *output) {
    text_buffer_assign(output, text);
}

static void append_text(char *buffer, size_t capacity, const char *text) {
    size_t used = strlen(buffer);
    size_t size = strlen(text);

    CHECK(size < capacity - used);
    memcpy(buffer + used, text, size + 1);
}

static void test_action_limit(void) {
    char input[2048] = "button1=";
    int  i;

    for (i = 0; i < MAXACTIONS + 1; i++) {
        if (i)
            append_text(input, sizeof(input), ",");
        append_text(input, sizeof(input), "exit");
    }

    dzen.running = True;
    fill_ev_table(input);
    do_action(button1);
    CHECK(dzen.running == False);
    free_event_list();
}

static void test_option_limit(void) {
    char input[2048] = "button1=exit";
    int  i;

    for (i = 0; i < MAXOPTIONS + 1; i++) {
        char option[16];

        snprintf(option, sizeof(option), ":%d", i + 1);
        append_text(input, sizeof(input), option);
    }

    dzen.ret_val = 0;
    fill_ev_table(input);
    do_action(button1);
    CHECK(dzen.ret_val == 1);
    free_event_list();
}

static void check_scroll_range(int count, int first, int last, int scroll_to_end) {
    dzen.slave_win.max_lines      = 5;
    dzen.slave_win.tcnt           = count;
    dzen.slave_win.first_line_vis = -1;
    dzen.slave_win.last_line_vis  = -1;
    draw_body_calls               = 0;

    if (scroll_to_end)
        a_scrollend(NULL);
    else
        a_scrollhome(NULL);

    CHECK(dzen.slave_win.first_line_vis == first);
    CHECK(dzen.slave_win.last_line_vis == last);
    CHECK(draw_body_calls == 1);
}

static void test_scroll_endpoints(void) {
    check_scroll_range(0, 0, 0, 0);
    check_scroll_range(2, 0, 2, 0);
    check_scroll_range(5, 0, 5, 0);
    check_scroll_range(8, 0, 5, 0);

    check_scroll_range(0, 0, 0, 1);
    check_scroll_range(2, 0, 2, 1);
    check_scroll_range(5, 0, 5, 1);
    check_scroll_range(8, 3, 8, 1);
}

static void check_invalid_menu_selection(int first, int selected) {
    dzen.slave_win.first_line_vis = first;
    dzen.slave_win.sel_line       = selected;
    spawn_calls                   = 0;

    a_menuexec(NULL);

    CHECK(spawn_calls == 0);
    CHECK(dzen.slave_win.sel_line == -1);
}

static void test_menu_selection(void) {
    int i;

    dzen.slave_win.ismenu = True;
    dzen.slave_win.tcnt   = 3;
    dzen.slave_win.tsize  = 3;
    dzen.slave_win.tbuf   = calloc(3, sizeof(TextBuffer));
    CHECK(dzen.slave_win.tbuf != NULL);
    text_buffer_assign(&dzen.slave_win.tbuf[0], "first");
    text_buffer_assign(&dzen.slave_win.tbuf[1], "second");
    text_buffer_assign(&dzen.slave_win.tbuf[2], "third");

    check_invalid_menu_selection(0, -1);
    check_invalid_menu_selection(-1, 0);
    check_invalid_menu_selection(0, 3);
    check_invalid_menu_selection(3, 0);

    dzen.slave_win.first_line_vis = 1;
    dzen.slave_win.sel_line       = 1;
    spawn_calls                   = 0;
    a_menuexec(NULL);
    CHECK(spawn_calls == 1);
    CHECK(strcmp(spawned_command, "third") == 0);
    CHECK(dzen.slave_win.sel_line == -1);

    for (i = 0; i < dzen.slave_win.tsize; i++)
        text_buffer_destroy(&dzen.slave_win.tbuf[i]);
    free(dzen.slave_win.tbuf);
    dzen.slave_win.tbuf  = NULL;
    dzen.slave_win.tsize = 0;
}

static void test_event_names_are_exact(void) {
    CHECK(get_ev_id("button1") == button1);
    CHECK(get_ev_id("onstart") == onstart);
    CHECK(get_ev_id("button10") == -1);
    CHECK(get_ev_id("onstartup") == -1);
    CHECK(get_ev_id("sigusr123") == -1);
}

int main(void) {
    test_action_limit();
    test_option_limit();
    test_scroll_endpoints();
    test_menu_selection();
    test_event_names_are_exact();
    puts("action tests passed");
    return EXIT_SUCCESS;
}
