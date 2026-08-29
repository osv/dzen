#ifndef DZEN_TEXT_BUFFER_H
#define DZEN_TEXT_BUFFER_H

#include <stddef.h>

typedef struct {
    char  *data;
    size_t length;
    size_t capacity;
} TextBuffer;

static inline const char *text_buffer_data(const TextBuffer *buffer) {
    return buffer->data ? buffer->data : "";
}

void text_buffer_reserve(TextBuffer *buffer, size_t length);
void text_buffer_assign(TextBuffer *buffer, const char *text);
void text_buffer_assign_n(TextBuffer *buffer, const char *text, size_t length);
void text_buffer_append_n(TextBuffer *buffer, const char *text, size_t length);
void text_buffer_clear(TextBuffer *buffer);
void text_buffer_destroy(TextBuffer *buffer);

#endif
