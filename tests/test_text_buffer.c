#include "../src/line_reader.h"
#include "../src/text_buffer.h"
#include "test_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t count;
    size_t lengths[8];
    char   values[8][16];
    char   first[8];
    char   last[8];
} Lines;

static void record_line(const char *line, size_t length, void *context) {
    Lines *lines = context;
    size_t copy  = length < sizeof(lines->values[0]) - 1 ? length : sizeof(lines->values[0]) - 1;
    size_t index = lines->count;

    CHECK(index < sizeof(lines->lengths) / sizeof(lines->lengths[0]));
    lines->lengths[index] = length;
    memcpy(lines->values[index], line, copy);
    lines->values[index][copy] = '\0';
    lines->first[index]        = length ? line[0] : '\0';
    lines->last[index]         = length ? line[length - 1] : '\0';
    lines->count++;
}

static void test_text_buffer(void) {
    TextBuffer buffer   = { 0 };
    size_t     capacity = 0;
    char       binary[] = { 'a', '\0', 'b' };

    CHECK(strcmp(text_buffer_data(&buffer), "") == 0);
    CHECK(buffer.length == 0);
    CHECK(buffer.capacity == 0);

    text_buffer_assign(&buffer, "first value");
    CHECK(strcmp(text_buffer_data(&buffer), "first value") == 0);
    capacity = buffer.capacity;

    text_buffer_assign(&buffer, "short");
    CHECK(strcmp(text_buffer_data(&buffer), "short") == 0);
    CHECK(buffer.capacity == capacity);

    text_buffer_append_n(&buffer, " text", 5);
    CHECK(strcmp(text_buffer_data(&buffer), "short text") == 0);

    text_buffer_clear(&buffer);
    CHECK(buffer.length == 0);
    CHECK(buffer.capacity == capacity);
    CHECK(strcmp(text_buffer_data(&buffer), "") == 0);

    text_buffer_assign_n(&buffer, binary, sizeof(binary));
    CHECK(buffer.length == sizeof(binary));
    CHECK(memcmp(buffer.data, binary, sizeof(binary)) == 0);
    CHECK(buffer.data[sizeof(binary)] == '\0');

    text_buffer_reserve(&buffer, 4096);
    CHECK(buffer.capacity >= 4097);
    text_buffer_destroy(&buffer);
    CHECK(buffer.data == NULL);
    CHECK(buffer.length == 0);
    CHECK(buffer.capacity == 0);
}

static void test_text_buffer_aliasing(void) {
    TextBuffer buffer   = { 0 };
    char      *expected = NULL;
    size_t     original_length;
    size_t     original_capacity;
    size_t     index;

    text_buffer_assign(&buffer, "0123456789abcdef");
    text_buffer_assign_n(&buffer, buffer.data + 3, 10);
    CHECK(buffer.length == 10);
    CHECK(memcmp(buffer.data, "3456789abc", 10) == 0);
    CHECK(buffer.data[10] == '\0');

    text_buffer_assign(&buffer, "self-append");
    text_buffer_append_n(&buffer, buffer.data, buffer.length);
    CHECK(strcmp(buffer.data, "self-appendself-append") == 0);

    original_capacity = buffer.capacity;
    original_length   = original_capacity * 3 / 4;
    CHECK(original_length * 2 + 1 > original_capacity);
    expected = malloc(original_length * 2 + 1);
    CHECK(expected != NULL);
    for (index = 0; index < original_length; index++)
        expected[index] = (char)('a' + index % 26);
    memcpy(expected + original_length, expected, original_length);
    expected[original_length * 2] = '\0';

    text_buffer_assign_n(&buffer, expected, original_length);
    CHECK(buffer.capacity == original_capacity);
    text_buffer_append_n(&buffer, buffer.data, buffer.length);
    CHECK(buffer.length == original_length * 2);
    CHECK(memcmp(buffer.data, expected, original_length * 2 + 1) == 0);

    free(expected);
    text_buffer_destroy(&buffer);
}

static void test_line_boundaries(void) {
    LineReader reader = { 0 };
    Lines      lines  = { 0 };

    line_reader_feed(&reader, "one\ntw", 6, record_line, &lines);
    line_reader_feed(&reader, "o\n\npartial", 10, record_line, &lines);

    CHECK(lines.count == 3);
    CHECK(lines.lengths[0] == 3);
    CHECK(strcmp(lines.values[0], "one") == 0);
    CHECK(lines.lengths[1] == 3);
    CHECK(strcmp(lines.values[1], "two") == 0);
    CHECK(lines.lengths[2] == 0);
    CHECK(strcmp(lines.values[2], "") == 0);
    CHECK(reader.line.length == 7);
    CHECK(strcmp(text_buffer_data(&reader.line), "partial") == 0);

    line_reader_destroy(&reader);
}

static void test_line_limit_case(size_t input_length) {
    LineReader reader          = { 0 };
    Lines      lines           = { 0 };
    char      *chunk           = NULL;
    size_t     chunk_size      = 64U * 1024U;
    size_t     offset          = 0;
    size_t     retained_length = input_length < MAX_INPUT_LINE_BYTES ? input_length : MAX_INPUT_LINE_BYTES;

    chunk = malloc(chunk_size);
    CHECK(chunk != NULL);
    while (offset < input_length) {
        size_t length = input_length - offset;

        if (length > chunk_size)
            length = chunk_size;
        memset(chunk, 'x', length);
        if (offset <= retained_length - 1 && offset + length > retained_length - 1)
            chunk[retained_length - 1 - offset] = 'z';
        if (input_length > MAX_INPUT_LINE_BYTES && offset <= MAX_INPUT_LINE_BYTES &&
            offset + length > MAX_INPUT_LINE_BYTES)
            chunk[MAX_INPUT_LINE_BYTES - offset] = '!';
        line_reader_feed(&reader, chunk, length, record_line, &lines);
        offset += length;
    }

    CHECK(lines.count == 0);
    CHECK(reader.line.length == retained_length);
    CHECK(reader.truncated == (input_length > MAX_INPUT_LINE_BYTES));
    CHECK(reader.line.data[0] == 'x');
    CHECK(reader.line.data[retained_length - 1] == 'z');
    CHECK(reader.line.data[retained_length] == '\0');

    line_reader_feed(&reader, "\nnext\n", 6, record_line, &lines);
    CHECK(lines.count == 2);
    CHECK(lines.lengths[0] == retained_length);
    CHECK(lines.first[0] == 'x');
    CHECK(lines.last[0] == 'z');
    CHECK(lines.lengths[1] == 4);
    CHECK(strcmp(lines.values[1], "next") == 0);
    CHECK(reader.line.length == 0);
    CHECK(!reader.truncated);

    free(chunk);
    line_reader_destroy(&reader);
}

static void test_line_limits(void) {
    test_line_limit_case(MAX_INPUT_LINE_BYTES - 1);
    test_line_limit_case(MAX_INPUT_LINE_BYTES);
    test_line_limit_case(MAX_INPUT_LINE_BYTES + 1);
}

int main(void) {
    test_text_buffer();
    test_text_buffer_aliasing();
    test_line_boundaries();
    test_line_limits();
    puts("text buffer tests passed");
    return EXIT_SUCCESS;
}
