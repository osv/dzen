#include "line_reader.h"

#include <string.h>

static void append_segment(LineReader *reader, const char *data, size_t length) {
    size_t available;

    if (reader->truncated)
        return;

    available = MAX_INPUT_LINE_BYTES - reader->line.length;
    if (length > available) {
        text_buffer_append_n(&reader->line, data, available);
        reader->truncated = 1;
    } else {
        text_buffer_append_n(&reader->line, data, length);
    }
}

void line_reader_feed(LineReader *reader, const char *data, size_t length, LineReaderCallback callback, void *context) {
    size_t offset = 0;

    while (offset < length) {
        const char *newline = memchr(data + offset, '\n', length - offset);
        size_t      segment = newline ? (size_t)(newline - (data + offset)) : length - offset;

        append_segment(reader, data + offset, segment);
        if (!newline)
            return;

        callback(text_buffer_data(&reader->line), reader->line.length, context);
        text_buffer_clear(&reader->line);
        reader->truncated = 0;
        offset += segment + 1;
    }
}

void line_reader_destroy(LineReader *reader) {
    text_buffer_destroy(&reader->line);
    reader->truncated = 0;
}
