#include "text_buffer.h"
#include "util.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_TEXT_CAPACITY 64U

void text_buffer_reserve(TextBuffer *buffer, size_t length) {
    size_t capacity;
    char  *data;

    if (length == SIZE_MAX)
        eprint("fatal: text buffer size overflow\n");

    length++;
    if (length <= buffer->capacity)
        return;

    capacity = buffer->capacity ? buffer->capacity : INITIAL_TEXT_CAPACITY;
    while (capacity < length) {
        if (capacity > SIZE_MAX / 2) {
            capacity = length;
            break;
        }
        capacity *= 2;
    }

    data = realloc(buffer->data, capacity);
    if (!data)
        eprint("fatal: could not allocate %zu bytes for text buffer\n", capacity);

    buffer->data     = data;
    buffer->capacity = capacity;
}

void text_buffer_assign(TextBuffer *buffer, const char *text) {
    text_buffer_assign_n(buffer, text, strlen(text));
}

void text_buffer_assign_n(TextBuffer *buffer, const char *text, size_t length) {
    text_buffer_reserve(buffer, length);
    if (length)
        memcpy(buffer->data, text, length);
    buffer->data[length] = '\0';
    buffer->length       = length;
}

void text_buffer_append_n(TextBuffer *buffer, const char *text, size_t length) {
    if (length > SIZE_MAX - buffer->length)
        eprint("fatal: text buffer size overflow\n");

    text_buffer_reserve(buffer, buffer->length + length);
    if (length)
        memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

void text_buffer_clear(TextBuffer *buffer) {
    buffer->length = 0;
    if (buffer->data)
        buffer->data[0] = '\0';
}

void text_buffer_destroy(TextBuffer *buffer) {
    free(buffer->data);
    buffer->data     = NULL;
    buffer->length   = 0;
    buffer->capacity = 0;
}
