/*
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "dzen.h"
#include "action.h"
#include "font.h"
#include "layout.h"
#include "xrandr.h"

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

Dzen                  dzen     = { 0 };
static int            last_cnt = 0;
typedef void          sigfunc(int);

static LayoutRequest  layout_request;
static ResolvedLayout current_layout;
static XRectangle     current_target;
static XRandRContext  xrandr_context;
static char          *output_name;
static Bool           output_connected;
static Bool           layout_initialized;
static Bool           xinescreen_explicit;
static Bool           list_outputs;
static int            use_ewmh_dock;
static Bool           title_mapped_before_disconnect = True;
static Bool           slave_mapped_before_disconnect;

static void           clean_up(void) {
    int i;

    free_event_list();
    free_all_caches();
    font_cleanup();

    XFreePixmap(dzen.dpy, dzen.title_win.drawable);
    if (dzen.slave_win.max_lines) {
        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            XFreePixmap(dzen.dpy, dzen.slave_win.drawable[i]);
            XDestroyWindow(dzen.dpy, dzen.slave_win.line[i]);
        }
        free(dzen.slave_win.line);
        XDestroyWindow(dzen.dpy, dzen.slave_win.win);
    }
    XFreeGC(dzen.dpy, dzen.gc);
    XFreeGC(dzen.dpy, dzen.rgc);
    XFreeGC(dzen.dpy, dzen.tgc);
    XFreeCursor(dzen.dpy, dzen.cursor_arrow);
    XFreeCursor(dzen.dpy, dzen.cursor_hand);
    XDestroyWindow(dzen.dpy, dzen.title_win.win);
    XCloseDisplay(dzen.dpy);
    if (dzen.fnt)
        free(dzen.fnt);
    if (dzen.bg)
        free(dzen.bg);
    if (dzen.fg)
        free(dzen.fg);
    free(dzen.title_text);
    if (dzen.title_win.name)
        free(dzen.title_win.name);
    if (dzen.slave_win.name)
        free(dzen.slave_win.name);
    free(output_name);
}

static void catch_sigusr1(int s) {
    (void)s;
    do_action(sigusr1);
}

static void catch_sigusr2(int s) {
    (void)s;
    do_action(sigusr2);
}

static void catch_sigterm(int s) {
    (void)s;
    do_action(onexit);
    clean_up();
}

static void catch_alrm(int s) {
    (void)s;
    do_action(onexit);
    clean_up();
    exit(0);
}

static sigfunc *setup_signal(int signr, sigfunc *shandler) {
    struct sigaction nh, oh;

    nh.sa_handler = shandler;
    sigemptyset(&nh.sa_mask);
    nh.sa_flags = 0;

    if (sigaction(signr, &nh, &oh) < 0)
        return SIG_ERR;

    return NULL;
}

/* Static buffer to preserve partial lines between read() calls */
static char partial_buf[MAX_LINE_LEN];
static int  partial_len      = 0;
static int  partial_overflow = 0; /* Set when line exceeds MAX_LINE_LEN */

/* Extract complete lines from buffer, preserving partial lines for next call
 * Returns offset to next unprocessed character, or 0 if no complete lines found
 */
static int  extract_line(const char *inbuf, char *outbuf, int start, int len) {
    const char *line_start = inbuf + start;
    int         remaining  = len - start;

    /* Find newline using memchr - much faster than character-by-character scan */
    const char *newline = memchr(line_start, '\n', remaining);

    if (!newline) {
        /* No complete line found - save partial line for next read() call */
        if (partial_overflow) {
            /* Already overflowed, skip until newline */
            return 0;
        }
        if (remaining > 0) {
            int space_left = MAX_LINE_LEN - 1 - partial_len;
            if (remaining <= space_left) {
                /* Fits in buffer */
                memcpy(partial_buf + partial_len, line_start, remaining);
                partial_len += remaining;
            } else {
                /* Overflow: save what we can, mark as overflowed */
                if (space_left > 0) {
                    memcpy(partial_buf + partial_len, line_start, space_left);
                    partial_len += space_left;
                }
                partial_overflow = 1;
            }
        }
        return 0;
    }

    /* Calculate line length */
    int line_len = newline - line_start;

    /* Check if we have a partial line from previous read */
    if (partial_len > 0 || partial_overflow) {
        /* Output the truncated line (partial_buf already has MAX_LINE_LEN-1 chars if overflowed) */
        if (partial_len > 0) {
            memcpy(outbuf, partial_buf, partial_len);
        }

        if (!partial_overflow) {
            /* No overflow - try to append current chunk */
            int total_len = partial_len + line_len;
            if (total_len >= MAX_LINE_LEN - 1) {
                /* Combined line too long */
                int copy_len = MAX_LINE_LEN - 1 - partial_len;
                if (copy_len > 0) {
                    memcpy(outbuf + partial_len, line_start, copy_len);
                }
                outbuf[MAX_LINE_LEN - 1] = '\0';
            } else {
                /* Combine partial + current line */
                memcpy(outbuf + partial_len, line_start, line_len);
                outbuf[total_len] = '\0';
            }
        } else {
            /* Was overflowed - just null-terminate what we have */
            outbuf[partial_len] = '\0';
        }

        partial_len      = 0;
        partial_overflow = 0;
        return start + line_len + 1;
    }

    /* Handle line truncation if needed */
    if (line_len >= MAX_LINE_LEN - 1) {
        /* Line too long: draw first MAX_LINE_LEN-1 chars, ignore the rest */
        memcpy(outbuf, line_start, MAX_LINE_LEN - 1);
        outbuf[MAX_LINE_LEN - 1] = '\0';
        /* Skip to position after newline, ignoring the truncated portion */
        return start + line_len + 1;
    }

    /* Copy complete line using memcpy - much faster than char-by-char */
    memcpy(outbuf, line_start, line_len);
    outbuf[line_len] = '\0';

    /* Return position after newline */
    return start + line_len + 1;
}

void free_buffer(void) {
    int i;
    for (i = 0; i < dzen.slave_win.tcnt; i++) {
        free(dzen.slave_win.tbuf[i]);
        dzen.slave_win.tbuf[i] = NULL;
    }
    dzen.slave_win.tcnt = dzen.slave_win.last_line_vis = last_cnt = 0;
}

static int read_stdin(void) {
    char    buf[MAX_LINE_LEN], retbuf[MAX_LINE_LEN];
    ssize_t n;
    int     n_off = 0;

    if (!(n = read(STDIN_FILENO, buf, sizeof buf))) {
        if (!dzen.ispersistent) {
            dzen.running = False;
            return -1;
        } else
            return -2;
    } else {
        /* Process only complete lines, ignore partial lines at buffer end */
        while (n_off < n) {
            int next_off = extract_line(buf, retbuf, n_off, n);
            if (next_off == 0) {
                /* No more complete lines in buffer */
                break;
            }
            n_off = next_off;

            if (!dzen.slave_win.ishmenu && dzen.tsupdate && dzen.slave_win.max_lines &&
                ((dzen.current_line == 0) || !(dzen.current_line % (dzen.slave_win.max_lines + 1))))
                drawheader(retbuf);
            else if (!dzen.slave_win.ishmenu && !dzen.tsupdate &&
                     ((dzen.current_line == 0) || !dzen.slave_win.max_lines))
                drawheader(retbuf);
            else
                drawbody(retbuf);
            dzen.current_line++;
        }
    }
    return 0;
}

static void x_hilight_line(int line) {
    drawtext(dzen.slave_win.tbuf[line + dzen.slave_win.first_line_vis], 1, line, dzen.slave_win.alignment);
    XCopyArea(dzen.dpy, dzen.slave_win.drawable[line], dzen.slave_win.line[line], dzen.gc, 0, 0, dzen.slave_win.width,
              dzen.line_height, 0, 0);
}

static void x_unhilight_line(int line) {
    drawtext(dzen.slave_win.tbuf[line + dzen.slave_win.first_line_vis], 0, line, dzen.slave_win.alignment);
    XCopyArea(dzen.dpy, dzen.slave_win.drawable[line], dzen.slave_win.line[line], dzen.rgc, 0, 0, dzen.slave_win.width,
              dzen.line_height, 0, 0);
}

void x_draw_body(void) {
    int i;
    dzen.x = 0;
    dzen.y = 0;
    dzen.w = dzen.slave_win.width;
    dzen.h = dzen.line_height;

    window_sens[SLAVEWINDOW].sens_areas_cnt = 0;

    if (!dzen.slave_win.last_line_vis) {
        if (dzen.slave_win.tcnt < dzen.slave_win.max_lines) {
            dzen.slave_win.first_line_vis = 0;
            dzen.slave_win.last_line_vis  = dzen.slave_win.tcnt;
        } else {
            dzen.slave_win.first_line_vis = dzen.slave_win.tcnt - dzen.slave_win.max_lines;
            dzen.slave_win.last_line_vis  = dzen.slave_win.tcnt;
        }
    }

    for (i = 0; i < dzen.slave_win.max_lines; i++) {
        if (i < dzen.slave_win.last_line_vis)
            drawtext(dzen.slave_win.tbuf[i + dzen.slave_win.first_line_vis], 0, i, dzen.slave_win.alignment);
    }
    for (i = 0; i < dzen.slave_win.max_lines; i++)
        XCopyArea(dzen.dpy, dzen.slave_win.drawable[i], dzen.slave_win.line[i], dzen.gc, 0, 0, dzen.slave_win.width,
                  dzen.line_height, 0, 0);
}

static void queryscreeninfo_no_xinerama(Display *dpy, XRectangle *rect) {
    rect->x      = 0;
    rect->y      = 0;
    rect->width  = DisplayWidth(dpy, DefaultScreen(dpy));
    rect->height = DisplayHeight(dpy, DefaultScreen(dpy));
}

#ifdef HAVE_XINERAMA
static void queryscreeninfo(Display *dpy, XRectangle *rect, int screen) {
    XineramaScreenInfo *xsi      = NULL;
    int                 nscreens = 1;

    if (XineramaIsActive(dpy))
        xsi = XineramaQueryScreens(dpy, &nscreens);

    if (xsi == NULL || screen > nscreens || screen <= 0) {
        queryscreeninfo_no_xinerama(dpy, rect);
    } else {
        rect->x      = xsi[screen - 1].x_org;
        rect->y      = xsi[screen - 1].y_org;
        rect->width  = xsi[screen - 1].width;
        rect->height = xsi[screen - 1].height;
    }

    if (xsi != NULL)
        XFree(xsi);
}
#endif

static void update_docking_struts(Bool enabled);
static void x_redraw(Window w);

static void query_root_geometry(XRectangle *rect) {
    XWindowAttributes attributes;

    if (XGetWindowAttributes(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), &attributes)) {
        rect->x      = 0;
        rect->y      = 0;
        rect->width  = attributes.width;
        rect->height = attributes.height;
    } else {
        queryscreeninfo_no_xinerama(dzen.dpy, rect);
    }
}

static int layout_slave_drawable_width(const ResolvedLayout *layout) {
    return layout_request.horizontal_menu ? layout->menu_last_width : layout->slave.width;
}

static void set_layout_fields(const ResolvedLayout *layout, Bool creating_windows) {
    dzen.title_win.x              = layout->title.x;
    dzen.title_win.y              = layout->title.y;
    dzen.title_win.width          = layout->title.width;
    dzen.title_win.height         = layout->title.height;
    dzen.title_win.x_right_corner = layout->title_right;
    dzen.slave_win.x              = layout->slave.x;
    dzen.slave_win.y              = layout->slave.y;
    dzen.slave_win.height         = layout->slave.height;
    dzen.slave_win.width          = creating_windows && layout_request.horizontal_menu ? layout->slave.width
                                                                                       : layout_slave_drawable_width(layout);
}

static Pixmap resized_pixmap(Pixmap old, unsigned int old_width, unsigned int new_width) {
    Window root = RootWindow(dzen.dpy, dzen.screen);
    Pixmap replacement;

    replacement = XCreatePixmap(dzen.dpy, root, new_width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
    XFillRectangle(dzen.dpy, replacement, dzen.rgc, 0, 0, new_width, dzen.line_height);
    if (old != None) {
        XCopyArea(dzen.dpy, old, replacement, dzen.gc, 0, 0, old_width < new_width ? old_width : new_width,
                  dzen.line_height, 0, 0);
        XFreePixmap(dzen.dpy, old);
    }
    return replacement;
}

static void resize_layout_drawables(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout) {
    int old_slave_width = layout_slave_drawable_width(old_layout);
    int new_slave_width = layout_slave_drawable_width(new_layout);
    int i;

    if (old_layout->title.width != new_layout->title.width)
        dzen.title_win.drawable =
            resized_pixmap(dzen.title_win.drawable, old_layout->title.width, new_layout->title.width);
    if (dzen.slave_win.max_lines && old_slave_width != new_slave_width) {
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            dzen.slave_win.drawable[i] = resized_pixmap(dzen.slave_win.drawable[i], old_slave_width, new_slave_width);
    }
}

static void apply_layout(const ResolvedLayout *next) {
    ResolvedLayout old = current_layout;
    int            i;

    if (layout_initialized && layout_equal(&old, next))
        return;

    if (!layout_initialized) {
        current_layout     = *next;
        layout_initialized = True;
        set_layout_fields(next, True);
        return;
    }

    resize_layout_drawables(&old, next);
    current_layout = *next;
    set_layout_fields(next, False);

    XMoveResizeWindow(dzen.dpy, dzen.title_win.win, next->title.x, next->title.y, next->title.width,
                      dzen.title_win.ishidden && !layout_request.horizontal_menu ? 1 : next->title.height);
    if (dzen.slave_win.max_lines) {
        XMoveResizeWindow(dzen.dpy, dzen.slave_win.win, next->slave.x, next->slave.y, next->slave.width,
                          dzen.title_win.ishidden && layout_request.horizontal_menu ? 1 : next->slave.height);
        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            LayoutRect child;
            if (layout_request.horizontal_menu) {
                layout_menu_child(next, i, dzen.slave_win.max_lines, &child);
                XMoveResizeWindow(dzen.dpy, dzen.slave_win.line[i], child.x, child.y, child.width, child.height);
            } else {
                XMoveResizeWindow(dzen.dpy, dzen.slave_win.line[i], 0, i * dzen.line_height, next->slave.width,
                                  dzen.line_height);
            }
        }
    }

    update_docking_struts(output_connected || !output_name);
    redrawheader();
    if (dzen.slave_win.max_lines)
        x_draw_body();
}

static void remember_and_unmap_windows(void) {
    XWindowAttributes attributes;

    if (XGetWindowAttributes(dzen.dpy, dzen.title_win.win, &attributes))
        title_mapped_before_disconnect = attributes.map_state != IsUnmapped;
    if (dzen.slave_win.max_lines && XGetWindowAttributes(dzen.dpy, dzen.slave_win.win, &attributes))
        slave_mapped_before_disconnect = attributes.map_state != IsUnmapped;

    XUnmapWindow(dzen.dpy, dzen.title_win.win);
    if (dzen.slave_win.max_lines)
        XUnmapWindow(dzen.dpy, dzen.slave_win.win);
    update_docking_struts(False);
}

static void restore_window_mapping(void) {
    int i;

    if (layout_request.horizontal_menu) {
        XMapRaised(dzen.dpy, dzen.slave_win.win);
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            XMapWindow(dzen.dpy, dzen.slave_win.line[i]);
    } else {
        if (title_mapped_before_disconnect)
            XMapRaised(dzen.dpy, dzen.title_win.win);
        if (slave_mapped_before_disconnect)
            XMapRaised(dzen.dpy, dzen.slave_win.win);
    }
    update_docking_struts(True);
}

static void handle_xrandr_event(XEvent *event) {
    XRectangle         target;
    ResolvedLayout     next;
    XRandROutputStatus status;
    Bool               was_connected = output_connected;

    if (xrandr_is_screen_change(&xrandr_context, event))
        xrandr_update_configuration(event);

    if (xinescreen_explicit)
        return;

    if (output_name) {
        status = xrandr_query_output(dzen.dpy, dzen.screen, output_name, &target);
        if (status == XRANDR_OUTPUT_QUERY_ERROR)
            return;
        if (status != XRANDR_OUTPUT_CONNECTED) {
            if (output_connected) {
                output_connected = False;
                remember_and_unmap_windows();
            }
            return;
        }
        output_connected = True;
    } else {
        query_root_geometry(&target);
    }

    current_target = target;
    layout_resolve(&layout_request, &target, &next);
    apply_layout(&next);
    if (output_name && !was_connected)
        restore_window_mapping();
}

static void set_docking_ewmh_info(Display *dpy, Window w, int dock) {
    Atom          type;
    unsigned int  desktop;
    pid_t         current_pid;
    char         *host_name;
    XTextProperty txt_prop;

    host_name = emalloc(HOST_NAME_MAX);
    if ((gethostname(host_name, HOST_NAME_MAX) > -1) && (current_pid = getpid())) {
        XStringListToTextProperty(&host_name, 1, &txt_prop);
        XSetWMClientMachine(dpy, w, &txt_prop);
        XFree(txt_prop.value);

        XChangeProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", False), XInternAtom(dpy, "CARDINAL", False), 32,
                        PropModeReplace, (unsigned char *)&current_pid, 1);
    }
    free(host_name);

    if (dock) {
        type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(dpy, w, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False), XInternAtom(dpy, "ATOM", False), 32,
                        PropModeReplace, (unsigned char *)&type, 1);

        /* some window managers honor this properties*/
        type = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(dpy, w, XInternAtom(dpy, "_NET_WM_STATE", False), XInternAtom(dpy, "ATOM", False), 32,
                        PropModeReplace, (unsigned char *)&type, 1);

        type = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
        XChangeProperty(dpy, w, XInternAtom(dpy, "_NET_WM_STATE", False), XInternAtom(dpy, "ATOM", False), 32,
                        PropModeAppend, (unsigned char *)&type, 1);

        desktop = 0xffffffff;
        XChangeProperty(dpy, w, XInternAtom(dpy, "_NET_WM_DESKTOP", False), XInternAtom(dpy, "CARDINAL", False), 32,
                        PropModeReplace, (unsigned char *)&desktop, 1);
    }
}

static void update_docking_struts(Bool enabled) {
    Atom          partial_atom = XInternAtom(dzen.dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom          strut_atom   = XInternAtom(dzen.dpy, "_NET_WM_STRUT", False);
    Atom          cardinal     = XInternAtom(dzen.dpy, "CARDINAL", False);
    unsigned long partial[12]  = { 0 };
    unsigned long strut[4]     = { 0 };
    XRectangle    root;

    if (!enabled || !use_ewmh_dock || !layout_initialized) {
        XDeleteProperty(dzen.dpy, dzen.title_win.win, partial_atom);
        XDeleteProperty(dzen.dpy, dzen.title_win.win, strut_atom);
        return;
    }

    query_root_geometry(&root);
    if (current_layout.title.y == current_target.y) {
        partial[2] = current_target.y + current_layout.title.height;
        partial[8] = current_layout.title.x;
        partial[9] = current_layout.title.x + current_layout.title.width - 1;
        strut[2]   = partial[2];
    } else if (current_layout.title.y + current_layout.title.height == current_target.y + current_target.height) {
        partial[3]  = root.height - (current_target.y + current_target.height) + current_layout.title.height;
        partial[10] = current_layout.title.x;
        partial[11] = current_layout.title.x + current_layout.title.width - 1;
        strut[3]    = partial[3];
    } else {
        XDeleteProperty(dzen.dpy, dzen.title_win.win, partial_atom);
        XDeleteProperty(dzen.dpy, dzen.title_win.win, strut_atom);
        return;
    }

    XChangeProperty(dzen.dpy, dzen.title_win.win, partial_atom, cardinal, 32, PropModeReplace, (unsigned char *)partial,
                    12);
    XChangeProperty(dzen.dpy, dzen.title_win.win, strut_atom, cardinal, 32, PropModeReplace, (unsigned char *)strut, 4);
}

static void x_create_gcs(void) {
    XGCValues gcv;
    gcv.graphics_exposures = 0;

    /* normal GC */
    dzen.gc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
    XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
    XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
    /* reverse color GC */
    dzen.rgc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
    XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
    XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
    /* temporary GC */
    dzen.tgc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
}

static void x_connect(void) {
    dzen.dpy = XOpenDisplay(0);
    if (!dzen.dpy)
        eprint("dzen: cannot open display\n");
    dzen.screen = DefaultScreen(dzen.dpy);
}

/* Read display styles from X resources. */
static void x_read_resources(void) {
    XrmDatabase xdb;
    char       *xrm;
    char       *datatype[20];
    XrmValue    xvalue;

    XrmInitialize();
    xrm = XResourceManagerString(dzen.dpy);
    if (xrm != NULL) {
        xdb = XrmGetStringDatabase(xrm);
        if (XrmGetResource(xdb, "dzen2.font", "*", datatype, &xvalue) == True) {
            if (dzen.fnt)
                free(dzen.fnt);
            dzen.fnt = estrdup(xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.foreground", "*", datatype, &xvalue) == True) {
            if (dzen.fg)
                free(dzen.fg);
            dzen.fg = estrdup(xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.background", "*", datatype, &xvalue) == True) {
            if (dzen.bg)
                free(dzen.bg);
            dzen.bg = estrdup(xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.titlename", "*", datatype, &xvalue) == True) {
            if (dzen.title_win.name)
                free(dzen.title_win.name);
            dzen.title_win.name = estrdup(xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.slavename", "*", datatype, &xvalue) == True) {
            if (dzen.slave_win.name)
                free(dzen.slave_win.name);
            dzen.slave_win.name = estrdup(xvalue.addr);
        }
        XrmDestroyDatabase(xdb);
    }
}

static void x_create_windows(int use_ewmh_dock) {
    XSetWindowAttributes wa;
    Window               root;
    int                  i;
    XClassHint          *class_hint;

    root = RootWindow(dzen.dpy, dzen.screen);

    /* style */
    if ((dzen.norm[ColBG] = get_color(dzen.bg)) == ~0lu)
        eprint("dzen: error, cannot allocate color '%s'\n", dzen.bg);
    if ((dzen.norm[ColFG] = get_color(dzen.fg)) == ~0lu)
        eprint("dzen: error, cannot allocate color '%s'\n", dzen.fg);

    x_create_gcs();

    /* window attributes */
    wa.override_redirect = (use_ewmh_dock ? 0 : 1);
    wa.background_pixmap = ParentRelative;
    wa.event_mask        = ExposureMask | ButtonReleaseMask | ButtonPressMask | ButtonMotionMask | EnterWindowMask |
                    LeaveWindowMask | KeyPressMask | PointerMotionMask;

    /* title window */
    dzen.title_win.win = XCreateWindow(dzen.dpy, root, dzen.title_win.x, dzen.title_win.y, dzen.title_win.width,
                                       dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                       DefaultVisual(dzen.dpy, dzen.screen),
                                       CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    /* set class property */
    class_hint            = XAllocClassHint();
    class_hint->res_name  = "dzen2";
    class_hint->res_class = "dzen";
    XSetClassHint(dzen.dpy, dzen.title_win.win, class_hint);
    XFree(class_hint);

    /* title */
    XStoreName(dzen.dpy, dzen.title_win.win, dzen.title_win.name);

    dzen.title_win.drawable =
        XCreatePixmap(dzen.dpy, root, dzen.title_win.width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
    XFillRectangle(dzen.dpy, dzen.title_win.drawable, dzen.rgc, 0, 0, dzen.title_win.width, dzen.line_height);

    /* set some hints for windowmanagers*/
    set_docking_ewmh_info(dzen.dpy, dzen.title_win.win, use_ewmh_dock);

    /* TODO: Smarter approach to window creation so we can reduce the
     *       size of this function.
     */

    if (dzen.slave_win.max_lines) {
        dzen.slave_win.first_line_vis = 0;
        dzen.slave_win.last_line_vis  = 0;
        dzen.slave_win.line           = emalloc(sizeof(Window) * dzen.slave_win.max_lines);
        dzen.slave_win.drawable       = emalloc(sizeof(Drawable) * dzen.slave_win.max_lines);

        /* horizontal menu mode */
        if (dzen.slave_win.ishmenu) {
            int parent_width        = current_layout.slave.width;
            int ew                  = current_layout.menu_entry_width;
            int last_width          = current_layout.menu_last_width;
            dzen.slave_win.issticky = True;

            dzen.slave_win.win = XCreateWindow(dzen.dpy, root, dzen.slave_win.x, dzen.slave_win.y, parent_width,
                                               dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                               DefaultVisual(dzen.dpy, dzen.screen),
                                               CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
            XStoreName(dzen.dpy, dzen.slave_win.win, dzen.slave_win.name);

            for (i = 0; i < dzen.slave_win.max_lines; i++) {
                dzen.slave_win.drawable[i] =
                    XCreatePixmap(dzen.dpy, root, last_width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
                XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, last_width, dzen.line_height);
            }

            /* windows holding the lines */
            for (i = 0; i < dzen.slave_win.max_lines; i++)
                dzen.slave_win.line[i] = XCreateWindow(dzen.dpy, dzen.slave_win.win, i * ew, 0,
                                                       (i == dzen.slave_win.max_lines - 1) ? last_width : ew,
                                                       dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen),
                                                       CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen),
                                                       CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

            /* As we don't use the title window in this mode,
             * we reuse its width value
             */
            dzen.title_win.width = parent_width;
            dzen.slave_win.width = last_width;
        }

        /* vertical slave window */
        else {
            dzen.slave_win.issticky = False;
            dzen.slave_win.win = XCreateWindow(dzen.dpy, root, dzen.slave_win.x, dzen.slave_win.y, dzen.slave_win.width,
                                               dzen.slave_win.max_lines * dzen.line_height, 0,
                                               DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                               DefaultVisual(dzen.dpy, dzen.screen),
                                               CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
            XStoreName(dzen.dpy, dzen.slave_win.win, dzen.slave_win.name);

            for (i = 0; i < dzen.slave_win.max_lines; i++) {
                dzen.slave_win.drawable[i] = XCreatePixmap(dzen.dpy, root, dzen.slave_win.width, dzen.line_height,
                                                           DefaultDepth(dzen.dpy, dzen.screen));
                XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, dzen.slave_win.width,
                               dzen.line_height);
            }

            /* windows holding the lines */
            for (i = 0; i < dzen.slave_win.max_lines; i++)
                dzen.slave_win.line[i] = XCreateWindow(dzen.dpy, dzen.slave_win.win, 0, i * dzen.line_height,
                                                       dzen.slave_win.width, dzen.line_height, 0,
                                                       DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                                       DefaultVisual(dzen.dpy, dzen.screen),
                                                       CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        }
    }
}

static void x_map_window(Window win) {
    XMapRaised(dzen.dpy, win);
    XSync(dzen.dpy, False);
}

static void x_redraw(Window w) {
    int i;

    if (!dzen.slave_win.ishmenu && w == dzen.title_win.win)
        drawheader(NULL);
    if (!dzen.tsupdate && w == dzen.slave_win.win) {
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            XCopyArea(dzen.dpy, dzen.slave_win.drawable[i], dzen.slave_win.line[i], dzen.gc, 0, 0, dzen.slave_win.width,
                      dzen.line_height, 0, 0);
    } else {
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            if (w == dzen.slave_win.line[i]) {
                XCopyArea(dzen.dpy, dzen.slave_win.drawable[i], dzen.slave_win.line[i], dzen.gc, 0, 0,
                          dzen.slave_win.width, dzen.line_height, 0, 0);
            }
    }
}

static int              timeout_active = 0;
static struct itimerval timer          = { 0 };

static void             reset_timer(void) {
    memset(&timer, 0, sizeof(timer));
    setitimer(ITIMER_REAL, &timer, NULL);
    timeout_active = 0;
}

static void start_timer(int seconds) {
    if (timeout_active)
        return;
    timer.it_value.tv_sec  = seconds;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
    timeout_active = 1;
}

static void handle_xev(void) {
    XEvent ev;
    int    i, sa_clicked = 0;
    char   buf[32];
    KeySym ksym;

    XNextEvent(dzen.dpy, &ev);

#ifdef HAVE_XRANDR
    if (xrandr_is_event(&xrandr_context, &ev)) {
        handle_xrandr_event(&ev);
        return;
    }
#endif

    switch (ev.type) {
    case Expose:
        if (ev.xexpose.count == 0)
            x_redraw(ev.xexpose.window);
        break;
    case EnterNotify:
        if (dzen.timeout > 0) {
            reset_timer();
        }
        if (dzen.slave_win.ismenu) {
            for (i = 0; i < dzen.slave_win.max_lines; i++)
                if (ev.xcrossing.window == dzen.slave_win.line[i])
                    x_hilight_line(i);
        }
        if (!dzen.slave_win.ishmenu && ev.xcrossing.window == dzen.title_win.win)
            do_action(entertitle);
        if (ev.xcrossing.window == dzen.slave_win.win)
            do_action(enterslave);
        break;
    case LeaveNotify:
        if (dzen.timeout > 0 && ev.xcrossing.detail != NotifyInferior) {
            start_timer(dzen.timeout);
        }
        if (dzen.slave_win.ismenu) {
            for (i = 0; i < dzen.slave_win.max_lines; i++)
                if (ev.xcrossing.window == dzen.slave_win.line[i])
                    x_unhilight_line(i);
        }
        if (!dzen.slave_win.ishmenu && ev.xcrossing.window == dzen.title_win.win) {
            do_action(leavetitle);
        }
        if (ev.xcrossing.window == dzen.slave_win.win) {
            do_action(leaveslave);
        }
        break;

    case ButtonRelease: {
        if (dzen.slave_win.ismenu) {
            for (i = 0; i < dzen.slave_win.max_lines; i++) {
                if (ev.xbutton.window == dzen.slave_win.line[i]) {
                    dzen.slave_win.sel_line = i;
                }
            }
        }

        /* clickable areas */
        int    w_id = ev.xbutton.window == dzen.title_win.win ? 0 : 1;
        sens_w w    = window_sens[w_id];
        for (i = w.sens_areas_cnt - 1; i >= 0; i--) {
            if (ev.xbutton.window == w.sens_areas[i].win && ev.xbutton.button == w.sens_areas[i].button &&
                (ev.xbutton.x >= w.sens_areas[i].start_x && ev.xbutton.x <= w.sens_areas[i].end_x) &&
                (ev.xbutton.y >= w.sens_areas[i].start_y && ev.xbutton.y <= w.sens_areas[i].end_y) &&
                w.sens_areas[i].active) {
                spawn(w.sens_areas[i].cmd);
                sa_clicked++;
                break;
            }
        }
        if (!sa_clicked) {
            switch (ev.xbutton.button) {
            case Button1:
                do_action(button1);
                break;
            case Button2:
                do_action(button2);
                break;
            case Button3:
                do_action(button3);
                break;
            case Button4:
                do_action(button4);
                break;
            case Button5:
                do_action(button5);
                break;
            case Button6:
                do_action(button6);
                break;
            case Button7:
                do_action(button7);
                break;
            }
        }
    } break;

    case KeyPress:
        XLookupString(&ev.xkey, buf, sizeof buf, &ksym, 0);
        do_action(ksym + keymarker);
        break;

    case MotionNotify: {
        int    w_id           = (ev.xmotion.window == dzen.title_win.win) ? 0 : 1;
        sens_w w              = window_sens[w_id];
        int    over_clickable = 0;
        for (i = 0; i < w.sens_areas_cnt; i++) {
            click_a *ca = &w.sens_areas[i];
            if (ca->active && ca->win == ev.xmotion.window && ev.xmotion.x >= ca->start_x &&
                ev.xmotion.x <= ca->end_x && ev.xmotion.y >= ca->start_y && ev.xmotion.y <= ca->end_y) {
                XDefineCursor(dzen.dpy, ev.xmotion.window, dzen.cursor_hand);
                over_clickable = 1;
                break;
            }
        }
        if (!over_clickable) {
            XDefineCursor(dzen.dpy, ev.xmotion.window, dzen.cursor_arrow);
        }
    } break;
    }
}

static void handle_newl(void) {
    XWindowAttributes wa;

    if (dzen.slave_win.max_lines && (dzen.slave_win.tcnt > last_cnt)) {
        do_action(onnewinput);

        if (XGetWindowAttributes(dzen.dpy, dzen.slave_win.win, &wa), wa.map_state != IsUnmapped
                                                                         /* autoscroll and redraw only if  we're
                                                                          * currently viewing the last line of input
                                                                          */
                                                                         &&
                                                                         (dzen.slave_win.last_line_vis == last_cnt)) {
            dzen.slave_win.first_line_vis = 0;
            dzen.slave_win.last_line_vis  = 0;
            x_draw_body();
        }
        /* needed for a_scrollhome */
        else if (wa.map_state != IsUnmapped && dzen.slave_win.last_line_vis == dzen.slave_win.max_lines)
            x_draw_body();
        /* forget state if window was unmapped */
        else if (wa.map_state == IsUnmapped || !dzen.slave_win.last_line_vis) {
            dzen.slave_win.first_line_vis = 0;
            dzen.slave_win.last_line_vis  = 0;
            x_draw_body();
        }
        last_cnt = dzen.slave_win.tcnt;
    }
}

static void event_loop(void) {
    int    xfd, ret, dr = 0;
    fd_set rmask;

    xfd = ConnectionNumber(dzen.dpy);
    while (dzen.running) {
        FD_ZERO(&rmask);
        FD_SET(xfd, &rmask);
        if (dr != -2)
            FD_SET(STDIN_FILENO, &rmask);

        while (XPending(dzen.dpy))
            handle_xev();

        ret = select(xfd + 1, &rmask, NULL, NULL, NULL);
        if (ret) {
            if (dr != -2 && FD_ISSET(STDIN_FILENO, &rmask)) {
                if ((dr = read_stdin()) == -1)
                    return;
                handle_newl();
            }
            if (FD_ISSET(xfd, &rmask))
                handle_xev();
        }
    }
    return;
}

/* Get alignment from character 'l'eft, 'r'ight and 'c'enter */
static char alignment_from_char(char align) {
    switch (align) {
    case 'l':
        return ALIGNLEFT;
    case 'r':
        return ALIGNRIGHT;
    case 'c':
        return ALIGNCENTER;
    default:
        return ALIGNCENTER;
    }
}

static void init_input_buffer(void) {
    if (MIN_BUF_SIZE % dzen.slave_win.max_lines)
        dzen.slave_win.tsize = MIN_BUF_SIZE + (dzen.slave_win.max_lines - (MIN_BUF_SIZE % dzen.slave_win.max_lines));
    else
        dzen.slave_win.tsize = MIN_BUF_SIZE;

    dzen.slave_win.tbuf = emalloc(dzen.slave_win.tsize * sizeof(char *));
}

int main(int argc, char *argv[]) {
    int   i;
    char *action_string = NULL;
    char *endptr, *fnpre = NULL;

    /* default values */
    dzen.title_win.name = estrdup("dzen title");
    dzen.slave_win.name = estrdup("dzen slave");
    dzen.current_line   = 0;
    dzen.ret_val        = 0;
    dzen.title_win.x = dzen.slave_win.x = 0;
    dzen.title_win.y                    = 0;
    dzen.title_win.width = dzen.slave_win.width = 0;
    dzen.title_win.alignment                    = ALIGNCENTER;
    dzen.slave_win.alignment                    = ALIGNLEFT;
    dzen.fnt                                    = estrdup(FONT);
    dzen.bg                                     = estrdup(BGCOLOR);
    dzen.fg                                     = estrdup(FGCOLOR);
    dzen.slave_win.max_lines                    = 0;
    dzen.running                                = True;
    dzen.xinescreen                             = 0;
    dzen.tsupdate                               = 0;
    dzen.line_height                            = 0;
    dzen.title_win.expand                       = noexpand;

    /* Connect to X server */
    x_connect();
    x_read_resources();

    /* cmdline args */
    for (i = 1; i < argc; i++)
        if (!strncmp(argv[i], "-l", 3)) {
            if (++i < argc) {
                dzen.slave_win.max_lines = atoi(argv[i]);
                if (dzen.slave_win.max_lines)
                    init_input_buffer();
            }
        } else if (!strncmp(argv[i], "-geometry", 10)) {
            if (++i < argc) {
                int          t;
                int          tx, ty;
                unsigned int tw, th;

                t = XParseGeometry(argv[i], &tx, &ty, &tw, &th);

                if (t & XValue)
                    dzen.title_win.x = tx;
                if (t & YValue) {
                    dzen.title_win.y = ty;
                    if (!ty && (t & YNegative))
                        /* -0 != +0 */
                        dzen.title_win.y = -1;
                }
                if (t & WidthValue)
                    dzen.title_win.width = (signed int)tw;
                if (t & WidthValue)
                    layout_request.title_width_explicit = True;
                if (t & HeightValue)
                    dzen.line_height = (signed int)th;
            }
        } else if (!strncmp(argv[i], "-u", 3)) {
            dzen.tsupdate = True;
        } else if (!strncmp(argv[i], "-expand", 8)) {
            if (++i < argc) {
                switch (argv[i][0]) {
                case 'l':
                    dzen.title_win.expand = left;
                    break;
                case 'c':
                    dzen.title_win.expand = both;
                    break;
                case 'r':
                    dzen.title_win.expand = right;
                    break;
                default:
                    dzen.title_win.expand = noexpand;
                }
            }
        } else if (!strncmp(argv[i], "-p", 3)) {
            dzen.ispersistent = True;
            if (i + 1 < argc) {
                dzen.timeout = strtoul(argv[i + 1], &endptr, 10);
                if (*endptr)
                    dzen.timeout = 0;
                else {
                    i++;
                    start_timer(dzen.timeout);
                }
            }
        } else if (!strncmp(argv[i], "-ta", 4)) {
            if (++i < argc)
                dzen.title_win.alignment = alignment_from_char(argv[i][0]);
        } else if (!strncmp(argv[i], "-sa", 4)) {
            if (++i < argc)
                dzen.slave_win.alignment = alignment_from_char(argv[i][0]);
        } else if (!strncmp(argv[i], "-m", 3)) {
            dzen.slave_win.ismenu = True;
            if (i + 1 < argc) {
                if (argv[i + 1][0] == 'v') {
                    ++i;
                } else {
                    dzen.slave_win.ishmenu = (argv[i + 1][0] == 'h') ? ++i, True : False;
                }
            }
        } else if (!strncmp(argv[i], "-fn", 4)) {
            if (++i < argc)
                dzen.fnt = estrdup(argv[i]);
        } else if (!strncmp(argv[i], "-e", 3)) {
            if (++i < argc)
                action_string = argv[i];
        } else if (!strncmp(argv[i], "-title-name", 12)) {
            if (++i < argc)
                dzen.title_win.name = argv[i];
        } else if (!strncmp(argv[i], "-slave-name", 12)) {
            if (++i < argc)
                dzen.slave_win.name = argv[i];
        } else if (!strncmp(argv[i], "-bg", 4)) {
            if (++i < argc)
                dzen.bg = estrdup(argv[i]);
        } else if (!strncmp(argv[i], "-fg", 4)) {
            if (++i < argc)
                dzen.fg = estrdup(argv[i]);
        } else if (!strncmp(argv[i], "-x", 3)) {
            if (++i < argc)
                dzen.title_win.x = dzen.slave_win.x = atoi(argv[i]);
        } else if (!strncmp(argv[i], "-y", 3)) {
            if (++i < argc)
                dzen.title_win.y = atoi(argv[i]);
        } else if (!strncmp(argv[i], "-w", 3)) {
            if (++i < argc) {
                dzen.slave_win.width                = atoi(argv[i]);
                layout_request.slave_width_explicit = True;
            }
        } else if (!strncmp(argv[i], "-h", 3)) {
            if (++i < argc)
                dzen.line_height = atoi(argv[i]);
        } else if (!strncmp(argv[i], "-tw", 4)) {
            if (++i < argc) {
                dzen.title_win.width                = atoi(argv[i]);
                layout_request.title_width_explicit = True;
            }
        } else if (!strncmp(argv[i], "-fn-preload", 12)) {
            if (++i < argc) {
                fnpre = estrdup(argv[i]);
            }
        }
#ifdef HAVE_XINERAMA
        else if (!strcmp(argv[i], "-xs")) {
            if (++i < argc) {
                dzen.xinescreen     = atoi(argv[i]);
                xinescreen_explicit = True;
            }
        }
#endif
#ifdef HAVE_XRANDR
        else if (!strcmp(argv[i], "-output")) {
            if (++i < argc)
                output_name = estrdup(argv[i]);
        } else if (!strcmp(argv[i], "-lm")) {
            list_outputs = True;
        }
#endif
        else if (!strncmp(argv[i], "-dock", 6))
            use_ewmh_dock = 1;
        else if (!strncmp(argv[i], "-v", 3)) {
            printf("dzen-" VERSION ", (C)opyright 2007-2009 Robert Manea");
            printf(", (C)opyright 2025 Olexandr Sydorchuk\n");
            printf("Enabled optional features: "
#ifdef HAVE_XPM
                   " XPM"
#endif
#ifdef HAVE_XFT
                   " XFT"
#endif
#ifdef HAVE_XINERAMA
                   " XINERAMA"
#endif
#ifdef HAVE_XCURSOR
                   " XCURSOR"
#endif
#ifdef HAVE_XRANDR
                   " XRANDR"
#endif
                   "\n");
            return EXIT_SUCCESS;
        } else
            eprint("usage: dzen2 [-v] [-p [seconds]] [-m [v|h]] [-ta <l|c|r>] [-sa <l|c|r>]\n"
                   "             [-x <pixel>] [-y <pixel>] [-w <pixel>] [-h <pixel>] [-tw <pixel>] [-u]\n"
                   "             [-e <string>] [-l <lines>]  [-fn <font>] [-bg <color>] [-fg <color>]\n"
                   "             [-geometry <geometry string>] [-expand <left|right>] [-dock]\n"
                   "             [-title-name <string>] [-slave-name <string>]\n"
#ifdef HAVE_XINERAMA
                   "             [-xs <screen>]\n"
#endif
#ifdef HAVE_XRANDR
                   "             [-output <name>] [-lm]\n"
#endif
            );

    if (output_name && xinescreen_explicit)
        eprint("dzen: -output and -xs are mutually exclusive\n");

    if (dzen.tsupdate && !dzen.slave_win.max_lines)
        dzen.tsupdate = False;

    if (!setlocale(LC_ALL, "") || !XSupportsLocale())
        puts("dzen: locale not available, expect problems with fonts.\n");

    if (action_string)
        fill_ev_table(action_string);
    else {
        if (!dzen.slave_win.max_lines) {
            char edef[] = "button3=exit:13";
            fill_ev_table(edef);
        } else if (dzen.slave_win.ishmenu) {
            char edef[] = "enterslave=grabkeys;leaveslave=ungrabkeys;"
                          "button4=scrollup;button5=scrolldown;"
                          "key_Left=scrollup;key_Right=scrolldown;"
                          "button1=menuexec;button3=exit:13;"
                          "key_Escape=ungrabkeys,exit";
            fill_ev_table(edef);
        } else {
            char edef[] = "entertitle=uncollapse,grabkeys;"
                          "enterslave=grabkeys;leaveslave=collapse,ungrabkeys;"
                          "button1=menuexec;button2=togglestick;button3=exit:13;"
                          "button4=scrollup;button5=scrolldown;"
                          "key_Up=scrollup;key_Down=scrolldown;"
                          "key_Escape=ungrabkeys,exit";
            fill_ev_table(edef);
        }
    }

    if ((find_event(onexit) != -1) && (setup_signal(SIGTERM, catch_sigterm) == SIG_ERR))
        fprintf(stderr, "dzen: error hooking SIGTERM\n");

    if ((find_event(sigusr1) != -1) && (setup_signal(SIGUSR1, catch_sigusr1) == SIG_ERR))
        fprintf(stderr, "dzen: error hooking SIGUSR1\n");

    if ((find_event(sigusr2) != -1) && (setup_signal(SIGUSR2, catch_sigusr2) == SIG_ERR))
        fprintf(stderr, "dzen: error hooking SIGUSR2\n");

    if (setup_signal(SIGALRM, catch_alrm) == SIG_ERR)
        fprintf(stderr, "dzen: error hooking SIGALARM\n");

    if (dzen.slave_win.ishmenu && !dzen.slave_win.max_lines)
        dzen.slave_win.max_lines = 1;

#ifdef HAVE_XCURSOR
    dzen.cursor_arrow = XcursorLibraryLoadCursor(dzen.dpy, "left_ptr");
    dzen.cursor_hand  = XcursorLibraryLoadCursor(dzen.dpy, "hand2");
#else
    dzen.cursor_arrow = XCreateFontCursor(dzen.dpy, XC_left_ptr);
    dzen.cursor_hand  = XCreateFontCursor(dzen.dpy, XC_hand2);
#endif

    init_all_caches();
    font_init(dzen.dpy, dzen.screen);
    font_set_default(dzen.fnt);

#ifdef HAVE_XRANDR
    if (!xrandr_initialize(dzen.dpy, dzen.screen, &xrandr_context) && (output_name || list_outputs))
        eprint("dzen: XRandR 1.2 or later is required for -output and -lm\n");
    if (list_outputs) {
        if (!xrandr_list_active_outputs(dzen.dpy, dzen.screen))
            eprint("dzen: cannot query XRandR outputs\n");
        XCloseDisplay(dzen.dpy);
        free(output_name);
        return EXIT_SUCCESS;
    }
#endif

    if (!dzen.line_height) {
        int font_height;
        font_get_dimensions(NULL, NULL, &font_height);
        dzen.line_height = font_height + 2;
    }

    if (xinescreen_explicit) {
#ifdef HAVE_XINERAMA
        queryscreeninfo(dzen.dpy, &current_target, dzen.xinescreen);
#else
        query_root_geometry(&current_target);
#endif
        output_connected = True;
    } else if (output_name) {
        XRandROutputStatus status = xrandr_query_output(dzen.dpy, dzen.screen, output_name, &current_target);
        if (status == XRANDR_OUTPUT_NOT_FOUND)
            eprint("dzen: output '%s' not found (use -lm to list active outputs)\n", output_name);
        if (status == XRANDR_OUTPUT_QUERY_ERROR)
            eprint("dzen: cannot query output '%s'\n", output_name);
        output_connected = status == XRANDR_OUTPUT_CONNECTED;
        if (!output_connected) {
            fprintf(stderr, "dzen: output '%s' is disconnected; windows will remain hidden\n", output_name);
            query_root_geometry(&current_target);
        }
    } else {
        query_root_geometry(&current_target);
        output_connected = True;
    }

    layout_request.x               = dzen.title_win.x;
    layout_request.y               = dzen.title_win.y;
    layout_request.title_width     = dzen.title_win.width;
    layout_request.slave_width     = dzen.slave_win.width;
    layout_request.line_height     = dzen.line_height;
    layout_request.max_lines       = dzen.slave_win.max_lines;
    layout_request.expand          = dzen.title_win.expand;
    layout_request.horizontal_menu = dzen.slave_win.ishmenu;
    layout_resolve(&layout_request, &current_target, &current_layout);
    layout_initialized = True;
    set_layout_fields(&current_layout, True);

    x_create_windows(use_ewmh_dock);
    update_docking_struts(output_connected);

    if ((!output_name || output_connected) && !dzen.slave_win.ishmenu)
        x_map_window(dzen.title_win.win);
    else if (!output_name || output_connected) {
        XMapRaised(dzen.dpy, dzen.slave_win.win);
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            XMapWindow(dzen.dpy, dzen.slave_win.line[i]);
    }

    if (fnpre != NULL) {
        font_preload(fnpre);
        free(fnpre);
    }

    do_action(onstart);

    if (output_name && !output_connected) {
        XWindowAttributes attributes;
        if (dzen.slave_win.max_lines && XGetWindowAttributes(dzen.dpy, dzen.slave_win.win, &attributes))
            slave_mapped_before_disconnect = attributes.map_state != IsUnmapped;
        XUnmapWindow(dzen.dpy, dzen.title_win.win);
        if (dzen.slave_win.max_lines)
            XUnmapWindow(dzen.dpy, dzen.slave_win.win);
        update_docking_struts(False);
    }

    /* main loop */
    event_loop();

    do_action(onexit);
    clean_up();

    if (dzen.ret_val)
        return dzen.ret_val;

    return EXIT_SUCCESS;
}
