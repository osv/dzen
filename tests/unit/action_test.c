#include "action.h"
#include "dzen.h"
#include "test_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Dzen  dzen = { 0 };

void *emalloc(unsigned int size) {
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
    (void)command;
}

void x_draw_body(void) {
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

int main(void) {
    test_action_limit();
    test_option_limit();
    puts("action tests passed");
    return EXIT_SUCCESS;
}
