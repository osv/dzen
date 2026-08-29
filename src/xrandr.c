#include "xrandr.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>

Bool xrandr_initialize(Display *display, int screen, XRandRContext *context) {
    int major;
    int minor;

    memset(context, 0, sizeof(*context));
    if (!XRRQueryExtension(display, &context->event_base, &context->error_base))
        return False;
    if (!XRRQueryVersion(display, &major, &minor) || major < 1 || (major == 1 && minor < 2))
        return False;

    XRRSelectInput(display, RootWindow(display, screen),
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask | RROutputChangeNotifyMask);
    context->available = True;
    return True;
}

Bool xrandr_is_event(const XRandRContext *context, const XEvent *event) {
    if (!context->available)
        return False;
    return event->type == context->event_base + RRScreenChangeNotify || event->type == context->event_base + RRNotify;
}

Bool xrandr_is_screen_change(const XRandRContext *context, const XEvent *event) {
    return context->available && event->type == context->event_base + RRScreenChangeNotify;
}

void xrandr_update_configuration(XEvent *event) {
    XRRUpdateConfiguration(event);
}

XRandROutputStatus xrandr_query_output(Display *display, int screen, const char *name, XRectangle *geometry) {
    XRRScreenResources *resources;
    XRandROutputStatus  status = XRANDR_OUTPUT_NOT_FOUND;
    int                 i;

    resources = XRRGetScreenResourcesCurrent(display, RootWindow(display, screen));
    if (!resources)
        return XRANDR_OUTPUT_QUERY_ERROR;

    for (i = 0; i < resources->noutput; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(display, resources, resources->outputs[i]);
        if (!output)
            continue;
        if ((int)strlen(name) == output->nameLen && !memcmp(output->name, name, output->nameLen)) {
            status = XRANDR_OUTPUT_DISCONNECTED;
            if (output->connection == RR_Connected && output->crtc != None) {
                XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, resources, output->crtc);
                if (!crtc) {
                    status = XRANDR_OUTPUT_QUERY_ERROR;
                } else if (crtc->width > 0 && crtc->height > 0) {
                    geometry->x      = crtc->x;
                    geometry->y      = crtc->y;
                    geometry->width  = crtc->width;
                    geometry->height = crtc->height;
                    status           = XRANDR_OUTPUT_CONNECTED;
                    XRRFreeCrtcInfo(crtc);
                } else {
                    XRRFreeCrtcInfo(crtc);
                }
            }
            XRRFreeOutputInfo(output);
            break;
        }
        XRRFreeOutputInfo(output);
    }

    XRRFreeScreenResources(resources);
    return status;
}

Bool xrandr_list_active_outputs(Display *display, int screen) {
    XRRScreenResources *resources;
    int                 i;

    resources = XRRGetScreenResourcesCurrent(display, RootWindow(display, screen));
    if (!resources)
        return False;

    for (i = 0; i < resources->noutput; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(display, resources, resources->outputs[i]);
        if (output && output->connection == RR_Connected && output->crtc != None) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, resources, output->crtc);
            if (crtc) {
                printf("%.*s (%ux%u+%d+%d)\n", output->nameLen, output->name, crtc->width, crtc->height, crtc->x,
                       crtc->y);
                XRRFreeCrtcInfo(crtc);
            }
        }
        if (output)
            XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(resources);
    return True;
}

#else

Bool xrandr_initialize(Display *display, int screen, XRandRContext *context) {
    (void)display;
    (void)screen;
    memset(context, 0, sizeof(*context));
    return False;
}

Bool xrandr_is_event(const XRandRContext *context, const XEvent *event) {
    (void)context;
    (void)event;
    return False;
}

Bool xrandr_is_screen_change(const XRandRContext *context, const XEvent *event) {
    (void)context;
    (void)event;
    return False;
}

void xrandr_update_configuration(XEvent *event) {
    (void)event;
}

XRandROutputStatus xrandr_query_output(Display *display, int screen, const char *name, XRectangle *geometry) {
    (void)display;
    (void)screen;
    (void)name;
    (void)geometry;
    return XRANDR_OUTPUT_QUERY_ERROR;
}

Bool xrandr_list_active_outputs(Display *display, int screen) {
    (void)display;
    (void)screen;
    return False;
}

#endif
