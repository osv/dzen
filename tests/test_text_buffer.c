#include "../src/line_reader.h"
#include "../src/text_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    size_t count;
    size_t lengths[8];
    char   values[8][16];
} Lines;

static void record_line(const char *line, size_t length, void *context) {
    Lines *lines = context;
    size_t copy  = length < sizeof(lines->values[0]) - 1 ? length : sizeof(lines->values[0]) - 1;

    assert(lines->count < sizeof(lines->lengths) / sizeof(lines->lengths[0]));
    lines->lengths[lines->count] = length;
    memcpy(lines->values[lines->count], line, copy);
    lines->values[lines->count][copy] = '\0';
    lines->count++;
}

static void test_text_buffer(void) {
    TextBuffer buffer = { 0 };
    size_t     capacity;
    char       binary[] = { 'a', '\0', 'b' };

    assert(strcmp(text_buffer_data(&buffer), "") == 0);
    assert(buffer.length == 0 && buffer.capacity == 0);

    text_buffer_assign(&buffer, "first value");
    assert(strcmp(text_buffer_data(&buffer), "first value") == 0);
    capacity = buffer.capacity;

    text_buffer_assign(&buffer, "short");
    assert(strcmp(text_buffer_data(&buffer), "short") == 0);
    assert(buffer.capacity == capacity);

    text_buffer_append_n(&buffer, " text", 5);
    assert(strcmp(text_buffer_data(&buffer), "short text") == 0);

    text_buffer_clear(&buffer);
    assert(buffer.length == 0 && buffer.capacity == capacity);
    assert(strcmp(text_buffer_data(&buffer), "") == 0);

    text_buffer_assign_n(&buffer, binary, sizeof(binary));
    assert(buffer.length == sizeof(binary));
    assert(memcmp(buffer.data, binary, sizeof(binary)) == 0);
    assert(buffer.data[sizeof(binary)] == '\0');

    text_buffer_reserve(&buffer, 4096);
    assert(buffer.capacity >= 4097);
    text_buffer_destroy(&buffer);
    assert(buffer.data == NULL && buffer.length == 0 && buffer.capacity == 0);
}

static void test_line_boundaries(void) {
    LineReader reader = { 0 };
    Lines      lines  = { 0 };

    line_reader_feed(&reader, "one\ntw", 6, record_line, &lines);
    line_reader_feed(&reader, "o\n\npartial", 10, record_line, &lines);

    assert(lines.count == 3);
    assert(lines.lengths[0] == 3 && strcmp(lines.values[0], "one") == 0);
    assert(lines.lengths[1] == 3 && strcmp(lines.values[1], "two") == 0);
    assert(lines.lengths[2] == 0 && strcmp(lines.values[2], "") == 0);
    assert(reader.line.length == 7);
    assert(strcmp(text_buffer_data(&reader.line), "partial") == 0);

    line_reader_destroy(&reader);
}

static void test_line_limit(void) {
    LineReader reader = { 0 };
    Lines      lines  = { 0 };
    char       chunk[64 * 1024];
    size_t     bytes_remaining = MAX_INPUT_LINE_BYTES + sizeof(chunk);
    size_t     retained_capacity;

    memset(chunk, 'x', sizeof(chunk));
    while (bytes_remaining) {
        size_t length = bytes_remaining < sizeof(chunk) ? bytes_remaining : sizeof(chunk);
        line_reader_feed(&reader, chunk, length, record_line, &lines);
        bytes_remaining -= length;
    }

    assert(lines.count == 0);
    assert(reader.line.length == MAX_INPUT_LINE_BYTES);
    assert(reader.truncated);
    retained_capacity = reader.line.capacity;

    line_reader_feed(&reader, "\nnext\n", 6, record_line, &lines);
    assert(lines.count == 2);
    assert(lines.lengths[0] == MAX_INPUT_LINE_BYTES);
    assert(lines.values[0][0] == 'x');
    assert(lines.lengths[1] == 4 && strcmp(lines.values[1], "next") == 0);
    assert(reader.line.capacity == retained_capacity);
    assert(reader.line.length == 0 && !reader.truncated);

    line_reader_destroy(&reader);
}

int main(void) {
    test_text_buffer();
    test_line_boundaries();
    test_line_limit();
    puts("text buffer tests passed");
    return 0;
}
