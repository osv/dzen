/*
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "dzen.h"
#include "action.h"
#include "font.h"
#include "layout.h"
#include "line_reader.h"
#include "signal_dispatch.h"
#include "windows.h"
#include "xrandr.h"

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

Dzen                  dzen     = { 0 };
static int            last_cnt = 0;

static LayoutRequest  layout_request;
static ResolvedLayout current_layout;
static XRectangle     current_target;
static XRandRContext  xrandr_context;
static TextBuffer     output_name;
static Bool           output_connected;
static Bool           layout_initialized;
static Bool           xinescreen_explicit;
static Bool           list_outputs;
static int            use_ewmh_dock;
static LineReader     stdin_reader;
static SignalDispatch signal_dispatch;

enum ExitReason { EXIT_REASON_NORMAL, EXIT_REASON_SIGTERM, EXIT_REASON_TIMEOUT };

static Bool has_output_name(void) {
    return output_name.length != 0;
}

static void destroy_text_state(void) {
    int i;

    for (i = 0; i < dzen.slave_win.tsize; i++)
        text_buffer_destroy(&dzen.slave_win.tbuf[i]);
    free(dzen.slave_win.tbuf);
    dzen.slave_win.tbuf  = NULL;
    dzen.slave_win.tsize = 0;

    text_buffer_destroy(&dzen.fnt);
    text_buffer_destroy(&dzen.bg);
    text_buffer_destroy(&dzen.fg);
    text_buffer_destroy(&dzen.title_text);
    text_buffer_destroy(&dzen.title_win.name);
    text_buffer_destroy(&dzen.slave_win.name);
    text_buffer_destroy(&output_name);
    border_spec_destroy(&dzen.border);
    line_reader_destroy(&stdin_reader);
}

static void clean_up(void) {
    free_event_list();
    draw_cleanup();
    free_all_caches();
    font_cleanup();
    windows_destroy();
    XCloseDisplay(dzen.dpy);
    destroy_text_state();
}

void free_buffer(void) {
    int i;
    for (i = 0; i < dzen.slave_win.tcnt; i++)
        text_buffer_clear(&dzen.slave_win.tbuf[i]);
    dzen.slave_win.tcnt = dzen.slave_win.last_line_vis = last_cnt = 0;
}

static void process_input_line(const char *line, size_t length, void *context) {
    (void)length;
    (void)context;

    if (!dzen.slave_win.ishmenu && dzen.tsupdate && dzen.slave_win.max_lines &&
        ((dzen.current_line == 0) || !(dzen.current_line % (dzen.slave_win.max_lines + 1))))
        drawheader(line);
    else if (!dzen.slave_win.ishmenu && !dzen.tsupdate && ((dzen.current_line == 0) || !dzen.slave_win.max_lines))
        drawheader(line);
    else
        drawbody((char *)line);
    dzen.current_line++;
}

static int read_stdin(void) {
    char    chunk[16384];
    ssize_t length;

    length = read(STDIN_FILENO, chunk, sizeof(chunk));

    if (length < 0 && errno == EINTR)
        return -3;

    if (length < 0)
        eprint("dzen: cannot read stdin: %s\n", strerror(errno));

    if (length == 0) {
        if (!dzen.ispersistent) {
            dzen.running = False;
            return -1;
        } else
            return -2;
    }

    line_reader_feed(&stdin_reader, chunk, (size_t)length, process_input_line, NULL);
    return 0;
}

static void x_hilight_line(int line) {
    drawtext(text_buffer_data(&dzen.slave_win.tbuf[line + dzen.slave_win.first_line_vis]), 1, line,
             dzen.slave_win.alignment);
    XCopyArea(dzen.dpy, dzen.slave_win.drawable[line], dzen.slave_win.line[line], dzen.gc, 0, 0, dzen.slave_win.width,
              dzen.line_height, 0, 0);
}

static void x_unhilight_line(int line) {
    drawtext(text_buffer_data(&dzen.slave_win.tbuf[line + dzen.slave_win.first_line_vis]), 0, line,
             dzen.slave_win.alignment);
    XCopyArea(dzen.dpy, dzen.slave_win.drawable[line], dzen.slave_win.line[line], dzen.rgc, 0, 0, dzen.slave_win.width,
              dzen.line_height, 0, 0);
}

void x_draw_body(void) {
    int i, last_first_line;
    dzen.x = 0;
    dzen.y = 0;
    dzen.w = dzen.slave_win.width;
    dzen.h = dzen.line_height;

    window_sens[SLAVEWINDOW].sens_areas_cnt = 0;

    last_first_line = dzen.slave_win.tcnt > dzen.slave_win.max_lines ? dzen.slave_win.tcnt - dzen.slave_win.max_lines
                                                                     : 0;
    if (dzen.slave_win.scroll_mode == SCROLL_FOLLOW_END)
        dzen.slave_win.first_line_vis = last_first_line;
    else if (dzen.slave_win.first_line_vis > last_first_line)
        dzen.slave_win.first_line_vis = last_first_line;
    if (dzen.slave_win.first_line_vis < 0)
        dzen.slave_win.first_line_vis = 0;

    dzen.slave_win.last_line_vis = dzen.slave_win.first_line_vis + dzen.slave_win.max_lines;
    if (dzen.slave_win.last_line_vis > dzen.slave_win.tcnt)
        dzen.slave_win.last_line_vis = dzen.slave_win.tcnt;

    for (i = 0; i < dzen.slave_win.max_lines; i++) {
        if (i + dzen.slave_win.first_line_vis < dzen.slave_win.last_line_vis)
            drawtext(text_buffer_data(&dzen.slave_win.tbuf[i + dzen.slave_win.first_line_vis]), 0, i,
                     dzen.slave_win.alignment);
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

static void apply_layout(const ResolvedLayout *next) {
    ResolvedLayout old = current_layout;

    if (layout_initialized && layout_equal(&old, next))
        return;

    if (!layout_initialized) {
        current_layout     = *next;
        layout_initialized = True;
        windows_initialize_layout(next, layout_request.horizontal_menu);
        return;
    }

    windows_apply_layout(&old, next, layout_request.horizontal_menu);
    current_layout = *next;

    update_docking_struts(output_connected || !has_output_name());
    redrawheader();
    if (dzen.slave_win.max_lines)
        x_draw_body();
}

static void remember_and_unmap_windows(void) {
    windows_remember_and_unmap();
    update_docking_struts(False);
}

static void restore_window_mapping(void) {
    windows_restore_mapping();
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

    if (has_output_name()) {
        status = xrandr_query_output(dzen.dpy, dzen.screen, text_buffer_data(&output_name), &target);
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
    if (!layout_resolve(&layout_request, &target, &next))
        eprint("dzen: border/content geometry exceeds safe X11 dimensions\n");
    apply_layout(&next);
    if (has_output_name() && !was_connected)
        restore_window_mapping();
}

static void update_docking_struts(Bool enabled) {
    XRectangle root;

    if (!enabled || !use_ewmh_dock || !layout_initialized) {
        windows_update_docking_struts(NULL, NULL, NULL, False);
        return;
    }

    query_root_geometry(&root);
    windows_update_docking_struts(&current_layout, &current_target, &root, True);
}

void apply_border_spec(const char *text) {
    BorderSpec     replacement;
    BorderSpec     old;
    LayoutRequest  next_request;
    ResolvedLayout next_layout;
    long           pixel;

    border_spec_init(&replacement);
    if (!border_spec_parse(&replacement, text)) {
        border_spec_destroy(&replacement);
        return;
    }

    pixel = replacement.color_explicit ? get_color(replacement.color) : (long)dzen.norm[ColBG];
    if (pixel == -1)
        goto rejected;

    next_request        = layout_request;
    next_request.border = replacement.widths;
    if (!layout_resolve(&next_request, &current_target, &next_layout))
        goto rejected;

    old               = dzen.border;
    dzen.border       = replacement;
    layout_request    = next_request;
    dzen.border_pixel = (unsigned long)pixel;
    apply_layout(&next_layout);
    windows_set_outer_background(border_spec_visible(&dzen.border), dzen.border_pixel);
    border_spec_destroy(&old);
    return;

rejected:
    border_spec_destroy(&replacement);
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
            text_buffer_assign(&dzen.fnt, xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.foreground", "*", datatype, &xvalue) == True) {
            text_buffer_assign(&dzen.fg, xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.background", "*", datatype, &xvalue) == True) {
            text_buffer_assign(&dzen.bg, xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.titlename", "*", datatype, &xvalue) == True) {
            text_buffer_assign(&dzen.title_win.name, xvalue.addr);
        }
        if (XrmGetResource(xdb, "dzen2.slavename", "*", datatype, &xvalue) == True) {
            text_buffer_assign(&dzen.slave_win.name, xvalue.addr);
        }
        XrmDestroyDatabase(xdb);
    }
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
    Bool slave_mapped = False;

    if (dzen.slave_win.max_lines && (dzen.slave_win.tcnt > last_cnt)) {
        do_action(onnewinput);

        windows_slave_is_mapped(&slave_mapped);
        if (slave_mapped)
            x_draw_body();
        /* forget state if window was unmapped */
        else {
            dzen.slave_win.first_line_vis = 0;
            dzen.slave_win.last_line_vis  = 0;
            dzen.slave_win.scroll_mode    = SCROLL_FOLLOW_END;
            x_draw_body();
        }
        last_cnt = dzen.slave_win.tcnt;
    }
}

static enum ExitReason process_pending_signals(void) {
    unsigned int pending = signal_dispatch_take(&signal_dispatch);

    if (pending & SIGNAL_DISPATCH_TERM) {
        dzen.running = False;
        return EXIT_REASON_SIGTERM;
    }
    if (pending & SIGNAL_DISPATCH_ALRM) {
        dzen.running = False;
        return EXIT_REASON_TIMEOUT;
    }
    if (pending & SIGNAL_DISPATCH_USR1)
        do_action(sigusr1);
    if (pending & SIGNAL_DISPATCH_USR2)
        do_action(sigusr2);
    return EXIT_REASON_NORMAL;
}

static enum ExitReason event_loop(void) {
    enum { POLL_X, POLL_SIGNAL, POLL_STDIN, POLL_FD_COUNT };
    int             ret, dr = 0;
    enum ExitReason exit_reason;
    struct pollfd   fds[POLL_FD_COUNT];

    fds[POLL_X].fd          = ConnectionNumber(dzen.dpy);
    fds[POLL_X].events      = POLLIN;
    fds[POLL_SIGNAL].fd     = signal_dispatch_fd(&signal_dispatch);
    fds[POLL_SIGNAL].events = POLLIN;
    fds[POLL_STDIN].fd      = STDIN_FILENO;
    fds[POLL_STDIN].events  = POLLIN;
    for (;;) {
        exit_reason = process_pending_signals();
        if (exit_reason != EXIT_REASON_NORMAL)
            return exit_reason;
        if (!dzen.running)
            return EXIT_REASON_NORMAL;

        while (XPending(dzen.dpy))
            handle_xev();
        if (!dzen.running)
            return EXIT_REASON_NORMAL;

        fds[POLL_STDIN].fd = dr == -2 ? -1 : STDIN_FILENO;
        ret                = poll(fds, POLL_FD_COUNT, -1);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            eprint("dzen: poll failed: %s\n", strerror(errno));
        }
        if (ret > 0) {
            if (fds[POLL_X].revents & (POLLERR | POLLHUP | POLLNVAL))
                eprint("dzen: X connection polling failed\n");
            if (fds[POLL_SIGNAL].revents & (POLLERR | POLLHUP | POLLNVAL))
                eprint("dzen: signal pipe polling failed\n");
            if (fds[POLL_STDIN].revents & (POLLERR | POLLNVAL))
                eprint("dzen: stdin polling failed\n");
            if (fds[POLL_SIGNAL].revents & POLLIN) {
                exit_reason = process_pending_signals();
                if (exit_reason != EXIT_REASON_NORMAL)
                    return exit_reason;
            }
            if (dr != -2 && (fds[POLL_STDIN].revents & (POLLIN | POLLHUP))) {
                dr = read_stdin();
                if (dr == -1)
                    return EXIT_REASON_NORMAL;
                if (dr != -3)
                    handle_newl();
            }
            if (fds[POLL_X].revents & POLLIN)
                handle_xev();
        }
    }
    return EXIT_REASON_NORMAL;
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
    int i;

    for (i = 0; i < dzen.slave_win.tsize; i++)
        text_buffer_destroy(&dzen.slave_win.tbuf[i]);
    free(dzen.slave_win.tbuf);

    if (MIN_BUF_SIZE % dzen.slave_win.max_lines)
        dzen.slave_win.tsize = MIN_BUF_SIZE + (dzen.slave_win.max_lines - (MIN_BUF_SIZE % dzen.slave_win.max_lines));
    else
        dzen.slave_win.tsize = MIN_BUF_SIZE;

    dzen.slave_win.tbuf = calloc((size_t)dzen.slave_win.tsize, sizeof(TextBuffer));
    if (!dzen.slave_win.tbuf)
        eprint("fatal: could not allocate menu text buffers\n");
}

int main(int argc, char *argv[]) {
    int             i;
    char           *action_string = NULL;
    char           *endptr;
    enum ExitReason exit_reason;
    TextBuffer      fnpre = { 0 };

    /* default values */
    text_buffer_assign(&dzen.title_win.name, "dzen title");
    text_buffer_assign(&dzen.slave_win.name, "dzen slave");
    dzen.current_line = 0;
    dzen.ret_val      = 0;
    dzen.title_win.x = dzen.slave_win.x = 0;
    dzen.title_win.y                    = 0;
    dzen.title_win.width = dzen.slave_win.width = 0;
    dzen.title_win.alignment                    = ALIGNCENTER;
    dzen.slave_win.alignment                    = ALIGNLEFT;
    text_buffer_assign(&dzen.fnt, FONT);
    text_buffer_assign(&dzen.bg, BGCOLOR);
    text_buffer_assign(&dzen.fg, FGCOLOR);
    dzen.slave_win.max_lines   = 0;
    dzen.slave_win.sel_line    = -1;
    dzen.slave_win.scroll_mode = SCROLL_FOLLOW_END;
    dzen.running               = True;
    dzen.xinescreen            = 0;
    dzen.tsupdate              = 0;
    dzen.line_height           = 0;
    dzen.title_win.expand      = noexpand;
    border_spec_init(&dzen.border);

    /* Connect to X server */
    x_connect();
    x_read_resources();

    /* cmdline args */
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-b")) {
            if (++i >= argc)
                eprint("dzen: -b requires a border specification\n");
            if (!border_spec_parse(&dzen.border, argv[i]))
                eprint("dzen: invalid border specification '%s'\n", argv[i]);
        } else if (!strncmp(argv[i], "-l", 3)) {
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
                else
                    i++;
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
                text_buffer_assign(&dzen.fnt, argv[i]);
        } else if (!strncmp(argv[i], "-e", 3)) {
            if (++i < argc)
                action_string = argv[i];
        } else if (!strncmp(argv[i], "-title-name", 12)) {
            if (++i < argc)
                text_buffer_assign(&dzen.title_win.name, argv[i]);
        } else if (!strncmp(argv[i], "-slave-name", 12)) {
            if (++i < argc)
                text_buffer_assign(&dzen.slave_win.name, argv[i]);
        } else if (!strncmp(argv[i], "-bg", 4)) {
            if (++i < argc)
                text_buffer_assign(&dzen.bg, argv[i]);
        } else if (!strncmp(argv[i], "-fg", 4)) {
            if (++i < argc)
                text_buffer_assign(&dzen.fg, argv[i]);
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
            if (++i < argc)
                text_buffer_assign(&fnpre, argv[i]);
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
                text_buffer_assign(&output_name, argv[i]);
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
            XCloseDisplay(dzen.dpy);
            text_buffer_destroy(&fnpre);
            destroy_text_state();
            return EXIT_SUCCESS;
        } else
            eprint("usage: dzen2 [-v] [-p [seconds]] [-m [v|h]] [-ta <l|c|r>] [-sa <l|c|r>]\n"
                   "             [-x <pixel>] [-y <pixel>] [-w <pixel>] [-h <pixel>] [-tw <pixel>] [-u]\n"
                   "             [-e <string>] [-l <lines>] [-b <widths[,color]>] [-fn <font>]\n"
                   "             [-bg <color>] [-fg <color>]\n"
                   "             [-geometry <geometry string>] [-expand <left|right>] [-dock]\n"
                   "             [-title-name <string>] [-slave-name <string>]\n"
#ifdef HAVE_XINERAMA
                   "             [-xs <screen>]\n"
#endif
#ifdef HAVE_XRANDR
                   "             [-output <name>] [-lm]\n"
#endif
            );

    if (has_output_name() && xinescreen_explicit)
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

    if (dzen.slave_win.ishmenu && !dzen.slave_win.max_lines)
        dzen.slave_win.max_lines = 1;
    if (dzen.slave_win.max_lines && !dzen.slave_win.tbuf)
        init_input_buffer();

#ifdef HAVE_XCURSOR
    dzen.cursor_arrow = XcursorLibraryLoadCursor(dzen.dpy, "left_ptr");
    dzen.cursor_hand  = XcursorLibraryLoadCursor(dzen.dpy, "hand2");
#else
    dzen.cursor_arrow = XCreateFontCursor(dzen.dpy, XC_left_ptr);
    dzen.cursor_hand  = XCreateFontCursor(dzen.dpy, XC_hand2);
#endif

    init_all_caches();
    font_init(dzen.dpy, dzen.screen);
    font_set_default(text_buffer_data(&dzen.fnt));

#ifdef HAVE_XRANDR
    if (!xrandr_initialize(dzen.dpy, dzen.screen, &xrandr_context) && (has_output_name() || list_outputs))
        eprint("dzen: XRandR 1.2 or later is required for -output and -lm\n");
    if (list_outputs) {
        if (!xrandr_list_active_outputs(dzen.dpy, dzen.screen))
            eprint("dzen: cannot query XRandR outputs\n");
        font_cleanup();
        free_all_caches();
        XCloseDisplay(dzen.dpy);
        text_buffer_destroy(&fnpre);
        destroy_text_state();
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
    } else if (has_output_name()) {
        const char        *name   = text_buffer_data(&output_name);
        XRandROutputStatus status = xrandr_query_output(dzen.dpy, dzen.screen, name, &current_target);
        if (status == XRANDR_OUTPUT_NOT_FOUND)
            eprint("dzen: output '%s' not found (use -lm to list active outputs)\n", name);
        if (status == XRANDR_OUTPUT_QUERY_ERROR)
            eprint("dzen: cannot query output '%s'\n", name);
        output_connected = status == XRANDR_OUTPUT_CONNECTED;
        if (!output_connected) {
            fprintf(stderr, "dzen: output '%s' is disconnected; windows will remain hidden\n", name);
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
    layout_request.border          = dzen.border.widths;
    if (!layout_resolve(&layout_request, &current_target, &current_layout))
        eprint("dzen: border/content geometry exceeds safe X11 dimensions\n");
    layout_initialized = True;
    windows_initialize_layout(&current_layout, layout_request.horizontal_menu);

    windows_create(use_ewmh_dock, &current_layout);
    windows_set_output_available(output_connected);
    update_docking_struts(output_connected);

    if ((!has_output_name() || output_connected) && !dzen.slave_win.ishmenu)
        windows_map_title();
    else if (!has_output_name() || output_connected) {
        windows_map_slave();
    }

    if (fnpre.length)
        font_preload(text_buffer_data(&fnpre));
    text_buffer_destroy(&fnpre);

    if (signal_dispatch_init(&signal_dispatch, find_event(sigusr1) != -1, find_event(sigusr2) != -1) < 0)
        eprint("dzen: cannot initialize signal dispatcher: %s\n", strerror(errno));
    if (dzen.timeout > 0)
        start_timer(dzen.timeout);

    do_action(onstart);

    if (has_output_name() && !output_connected) {
        windows_remember_and_unmap();
        update_docking_struts(False);
    }

    /* main loop */
    exit_reason = event_loop();

    {
        unsigned int pending = signal_dispatch_shutdown(&signal_dispatch);

        if (pending & SIGNAL_DISPATCH_TERM)
            exit_reason = EXIT_REASON_SIGTERM;
        else if ((pending & SIGNAL_DISPATCH_ALRM) && exit_reason != EXIT_REASON_SIGTERM)
            exit_reason = EXIT_REASON_TIMEOUT;
    }
    do_action(onexit);
    clean_up();

    if (exit_reason == EXIT_REASON_SIGTERM)
        return 128 + SIGTERM;
    if (exit_reason == EXIT_REASON_TIMEOUT)
        return EXIT_SUCCESS;

    if (dzen.ret_val)
        return dzen.ret_val;

    return EXIT_SUCCESS;
}
