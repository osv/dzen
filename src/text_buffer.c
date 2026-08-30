#include "text_buffer.h"
#include "util.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_TEXT_CAPACITY 64U

static int text_buffer_source_offset(const TextBuffer *buffer, const char *text, size_t length, size_t *offset) {
    uintptr_t buffer_address;
    uintptr_t text_address;
    uintptr_t address_offset;

    if (!buffer->data || !text)
        return 0;

    buffer_address = (uintptr_t)(const void *)buffer->data;
    text_address   = (uintptr_t)(const void *)text;
    if (text_address < buffer_address)
        return 0;

    address_offset = text_address - buffer_address;
    if (address_offset > SIZE_MAX || (size_t)address_offset > buffer->length ||
        length > buffer->length - (size_t)address_offset)
        return 0;

    *offset = (size_t)address_offset;
    return 1;
}

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
    size_t source_offset = 0;
    int    source_is_internal;

    source_is_internal = text_buffer_source_offset(buffer, text, length, &source_offset);
    text_buffer_reserve(buffer, length);
    if (source_is_internal)
        text = buffer->data + source_offset;
    if (length)
        memmove(buffer->data, text, length);
    buffer->data[length] = '\0';
    buffer->length       = length;
}

void text_buffer_append_n(TextBuffer *buffer, const char *text, size_t length) {
    size_t old_length    = buffer->length;
    size_t source_offset = 0;
    int    source_is_internal;

    if (length > SIZE_MAX - buffer->length)
        eprint("fatal: text buffer size overflow\n");

    source_is_internal = text_buffer_source_offset(buffer, text, length, &source_offset);
    text_buffer_reserve(buffer, old_length + length);
    if (source_is_internal)
        text = buffer->data + source_offset;
    if (length)
        memmove(buffer->data + old_length, text, length);
    buffer->length               = old_length + length;
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
