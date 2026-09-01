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

void border_spec_init(BorderSpec *spec) {
    memset(spec, 0, sizeof(*spec));
}

void border_spec_destroy(BorderSpec *spec) {
    free(spec->color);
    border_spec_init(spec);
}

Bool border_spec_visible(const BorderSpec *spec) {
    return spec->widths.top != 0 || spec->widths.right != 0 || spec->widths.bottom != 0 || spec->widths.left != 0;
}

Bool border_spec_parse(BorderSpec *spec, const char *text) {
    char        *fields[MAX_BORDER_FIELDS] = { 0 };
    unsigned int widths[4];
    BorderSpec   replacement;
    const char  *field_begin;
    const char  *cursor;
    int          field_count = 0;
    int          width_count;
    int          i;
    Bool         valid = False;

    if (text == NULL || *text == '\0')
        return False;

    field_begin = text;
    for (cursor = text;; cursor++) {
        if (*cursor == ',' || *cursor == '\0') {
            const char *begin = field_begin;
            const char *end   = cursor;

            while (begin < end && isspace((unsigned char)*begin))
                begin++;
            while (end > begin && isspace((unsigned char)end[-1]))
                end--;
            if (begin == end || field_count == MAX_BORDER_FIELDS)
                goto done;
            fields[field_count] = duplicate_range(begin, end);
            if (fields[field_count] == NULL)
                goto done;
            field_count++;
            if (*cursor == '\0')
                break;
            field_begin = cursor + 1;
        }
    }

    if (field_count == 1)
        width_count = 1;
    else if (field_count == 2)
        width_count = parse_width(fields[1], &widths[1]) ? 2 : 1;
    else if (field_count == 3) {
        if (parse_width(fields[2], &widths[2]))
            goto done;
        width_count = 2;
    } else if (field_count == 4)
        width_count = 4;
    else if (field_count == 5)
        width_count = 4;
    else
        goto done;

    for (i = 0; i < width_count; i++) {
        if (!parse_width(fields[i], &widths[i]))
            goto done;
    }
    if (field_count != width_count && field_count != width_count + 1)
        goto done;

    border_spec_init(&replacement);
    if (width_count == 1) {
        replacement.widths.top = replacement.widths.right = replacement.widths.bottom = replacement.widths.left =
            widths[0];
    } else if (width_count == 2) {
        replacement.widths.top = replacement.widths.bottom = widths[0];
        replacement.widths.right = replacement.widths.left = widths[1];
    } else {
        replacement.widths.top    = widths[0];
        replacement.widths.right  = widths[1];
        replacement.widths.bottom = widths[2];
        replacement.widths.left   = widths[3];
    }
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
