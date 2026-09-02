#include "border.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_BORDER_FIELDS = 5 };

static char *duplicate_range(const char *begin, const char *end) {
    size_t length = (size_t)(end - begin);
    char  *result = malloc(length + 1);

    if (result == NULL)
        return NULL;
    memcpy(result, begin, length);
    result[length] = '\0';
    return result;
}

static Bool parse_width(const char *text, unsigned int *result) {
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned long        value;
    char                *end;

    if (*cursor == '\0')
        return False;
    while (*cursor != '\0') {
        if (!isdigit(*cursor))
            return False;
        cursor++;
    }

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value > UINT_MAX)
        return False;
    *result = (unsigned int)value;
    return True;
}

static void assign_widths(BoxInsets *insets, const unsigned int *widths, int count) {
    if (count == 1) {
        insets->top = insets->right = insets->bottom = insets->left = widths[0];
    } else if (count == 2) {
        insets->top = insets->bottom = widths[0];
        insets->right = insets->left = widths[1];
    } else {
        insets->top    = widths[0];
        insets->right  = widths[1];
        insets->bottom = widths[2];
        insets->left   = widths[3];
    }
}

static Bool parse_width_fields(char **fields, int count, BoxInsets *result) {
    unsigned int widths[4];
    int          i;

    if (count != 1 && count != 2 && count != 4)
        return False;
    for (i = 0; i < count; i++)
        if (!parse_width(fields[i], &widths[i]))
            return False;
    assign_widths(result, widths, count);
    return True;
}

static int split_fields(const char *text, char **fields, int capacity) {
    const char *field_begin;
    const char *cursor;
    int         count = 0;

    if (text == NULL || *text == '\0')
        return 0;
    field_begin = text;
    for (cursor = text;; cursor++) {
        if (*cursor == ',' || *cursor == '\0') {
            const char *begin = field_begin;
            const char *end   = cursor;

            while (begin < end && isspace((unsigned char)*begin))
                begin++;
            while (end > begin && isspace((unsigned char)end[-1]))
                end--;
            if (begin == end || count == capacity)
                return -1;
            fields[count] = duplicate_range(begin, end);
            if (fields[count] == NULL)
                return -1;
            count++;
            if (*cursor == '\0')
                return count;
            field_begin = cursor + 1;
        }
    }
}

Bool box_insets_parse(BoxInsets *insets, const char *text) {
    char     *fields[4] = { 0 };
    BoxInsets replacement;
    int       count;
    int       i;
    Bool      valid = False;

    count = split_fields(text, fields, 4);
    if (count > 0 && parse_width_fields(fields, count, &replacement)) {
        *insets = replacement;
        valid   = True;
    }
    for (i = 0; i < 4; i++)
        free(fields[i]);
    return valid;
}

Bool box_insets_visible(const BoxInsets *insets) {
    return insets->top != 0 || insets->right != 0 || insets->bottom != 0 || insets->left != 0;
}

void border_spec_init(BorderSpec *spec) {
    memset(spec, 0, sizeof(*spec));
}

void border_spec_destroy(BorderSpec *spec) {
    free(spec->color);
    border_spec_init(spec);
}

Bool border_spec_visible(const BorderSpec *spec) {
    return box_insets_visible(&spec->widths);
}

Bool border_spec_parse(BorderSpec *spec, const char *text) {
    char      *fields[MAX_BORDER_FIELDS] = { 0 };
    BorderSpec replacement;
    int        field_count;
    int        width_count;
    int        i;
    Bool       valid = False;

    field_count = split_fields(text, fields, MAX_BORDER_FIELDS);
    if (field_count <= 0)
        goto done;

    if (field_count == 1)
        width_count = 1;
    else if (field_count == 2)
        width_count = parse_width(fields[1], &(unsigned int){ 0 }) ? 2 : 1;
    else if (field_count == 3) {
        if (parse_width(fields[2], &(unsigned int){ 0 }))
            goto done;
        width_count = 2;
    } else if (field_count == 4)
        width_count = 4;
    else if (field_count == 5)
        width_count = 4;
    else
        goto done;

    if (field_count != width_count && field_count != width_count + 1)
        goto done;

    border_spec_init(&replacement);
    if (!parse_width_fields(fields, width_count, &replacement.widths))
        goto done;
    if (field_count == width_count + 1) {
        replacement.color = strdup(fields[width_count]);
        if (replacement.color == NULL)
            goto done;
        replacement.color_explicit = True;
    }

    border_spec_destroy(spec);
    *spec = replacement;
    valid = True;

done:
    for (i = 0; i < MAX_BORDER_FIELDS; i++)
        free(fields[i]);
    return valid;
}
