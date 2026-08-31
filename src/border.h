#ifndef DZEN_BORDER_H
#define DZEN_BORDER_H

#include <X11/Xlib.h>

typedef struct {
    unsigned int top;
    unsigned int right;
    unsigned int bottom;
    unsigned int left;
} BorderInsets;

typedef struct {
    BorderInsets widths;
    char        *color;
    Bool         color_explicit;
} BorderSpec;

void border_spec_init(BorderSpec *spec);
void border_spec_destroy(BorderSpec *spec);
Bool border_spec_parse(BorderSpec *spec, const char *text);
Bool border_spec_visible(const BorderSpec *spec);

#endif
