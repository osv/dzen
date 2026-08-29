#ifndef DZEN_LINE_READER_H
#define DZEN_LINE_READER_H

#include "text_buffer.h"

#include <stddef.h>

#define MAX_INPUT_LINE_BYTES (16U * 1024U * 1024U)

typedef void (*LineReaderCallback)(const char *line, size_t length, void *context);

typedef struct {
    TextBuffer line;
    int        truncated;
} LineReader;

void line_reader_feed(LineReader *reader, const char *data, size_t length, LineReaderCallback callback, void *context);
void line_reader_destroy(LineReader *reader);

#endif
