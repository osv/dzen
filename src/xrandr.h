#ifndef DZEN_XRANDR_H
#define DZEN_XRANDR_H

#include "../config.h"

#include <X11/Xlib.h>

typedef enum {
    XRANDR_OUTPUT_CONNECTED,
    XRANDR_OUTPUT_DISCONNECTED,
    XRANDR_OUTPUT_NOT_FOUND,
    XRANDR_OUTPUT_QUERY_ERROR
} XRandROutputStatus;

typedef struct {
    int  event_base;
    int  error_base;
    Bool available;
} XRandRContext;

Bool               xrandr_initialize(Display *display, int screen, XRandRContext *context);
Bool               xrandr_is_event(const XRandRContext *context, const XEvent *event);
Bool               xrandr_is_screen_change(const XRandRContext *context, const XEvent *event);
void               xrandr_update_configuration(XEvent *event);
XRandROutputStatus xrandr_query_output(Display *display, int screen, const char *name, XRectangle *geometry);
Bool               xrandr_list_active_outputs(Display *display, int screen);

#endif
