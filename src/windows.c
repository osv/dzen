#include "windows.h"

#include "dzen.h"

#include <stdlib.h>
#include <unistd.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

static int slave_drawable_width(const ResolvedLayout *layout, Bool horizontal_menu) {
    return horizontal_menu ? layout->menu_last_width : layout->slave.width;
}

static void set_layout_fields(const ResolvedLayout *layout, Bool horizontal_menu, Bool creating_windows) {
    dzen.title_win.x              = layout->title.x;
    dzen.title_win.y              = layout->title.y;
    dzen.title_win.width          = layout->title.width;
    dzen.title_win.height         = layout->title.height;
    dzen.title_win.x_right_corner = layout->title_right;
    dzen.slave_win.x              = layout->slave.x;
    dzen.slave_win.y              = layout->slave.y;
    dzen.slave_win.height         = layout->slave.height;
    dzen.slave_win.width          = creating_windows && horizontal_menu ? layout->slave.width
                                                                        : slave_drawable_width(layout, horizontal_menu);
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

static void resize_drawables(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout, Bool horizontal_menu) {
    int old_slave_width = slave_drawable_width(old_layout, horizontal_menu);
    int new_slave_width = slave_drawable_width(new_layout, horizontal_menu);
    int i;

    if (old_layout->title.width != new_layout->title.width)
        dzen.title_win.drawable =
            resized_pixmap(dzen.title_win.drawable, old_layout->title.width, new_layout->title.width);
    if (dzen.slave_win.max_lines && old_slave_width != new_slave_width) {
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            dzen.slave_win.drawable[i] = resized_pixmap(dzen.slave_win.drawable[i], old_slave_width, new_slave_width);
    }
}

static void set_docking_ewmh_info(Window window, Bool dock) {
    Atom          type;
    unsigned int  desktop;
    pid_t         current_pid;
    char         *host_name;
    XTextProperty txt_prop;

    host_name = emalloc(HOST_NAME_MAX);
    if ((gethostname(host_name, HOST_NAME_MAX) > -1) && (current_pid = getpid())) {
        XStringListToTextProperty(&host_name, 1, &txt_prop);
        XSetWMClientMachine(dzen.dpy, window, &txt_prop);
        XFree(txt_prop.value);

        XChangeProperty(dzen.dpy, window, XInternAtom(dzen.dpy, "_NET_WM_PID", False),
                        XInternAtom(dzen.dpy, "CARDINAL", False), 32, PropModeReplace, (unsigned char *)&current_pid,
                        1);
    }
    free(host_name);

    if (dock) {
        type = XInternAtom(dzen.dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(dzen.dpy, window, XInternAtom(dzen.dpy, "_NET_WM_WINDOW_TYPE", False),
                        XInternAtom(dzen.dpy, "ATOM", False), 32, PropModeReplace, (unsigned char *)&type, 1);

        type = XInternAtom(dzen.dpy, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(dzen.dpy, window, XInternAtom(dzen.dpy, "_NET_WM_STATE", False),
                        XInternAtom(dzen.dpy, "ATOM", False), 32, PropModeReplace, (unsigned char *)&type, 1);

        type = XInternAtom(dzen.dpy, "_NET_WM_STATE_STICKY", False);
        XChangeProperty(dzen.dpy, window, XInternAtom(dzen.dpy, "_NET_WM_STATE", False),
                        XInternAtom(dzen.dpy, "ATOM", False), 32, PropModeAppend, (unsigned char *)&type, 1);

        desktop = 0xffffffff;
        XChangeProperty(dzen.dpy, window, XInternAtom(dzen.dpy, "_NET_WM_DESKTOP", False),
                        XInternAtom(dzen.dpy, "CARDINAL", False), 32, PropModeReplace, (unsigned char *)&desktop, 1);
    }
}

static void create_gcs(void) {
    XGCValues gcv;
    gcv.graphics_exposures = 0;

    dzen.gc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
    XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
    XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
    dzen.rgc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
    XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
    XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
    dzen.tgc = XCreateGC(dzen.dpy, RootWindow(dzen.dpy, dzen.screen), GCGraphicsExposures, &gcv);
}

void windows_initialize_layout(const ResolvedLayout *layout, Bool horizontal_menu) {
    set_layout_fields(layout, horizontal_menu, True);
}

void windows_apply_layout(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout, Bool horizontal_menu,
                          Bool title_hidden) {
    int i;

    resize_drawables(old_layout, new_layout, horizontal_menu);
    set_layout_fields(new_layout, horizontal_menu, False);

    XMoveResizeWindow(dzen.dpy, dzen.title_win.win, new_layout->title.x, new_layout->title.y, new_layout->title.width,
                      title_hidden && !horizontal_menu ? 1 : new_layout->title.height);
    if (!dzen.slave_win.max_lines)
        return;

    XMoveResizeWindow(dzen.dpy, dzen.slave_win.win, new_layout->slave.x, new_layout->slave.y, new_layout->slave.width,
                      title_hidden && horizontal_menu ? 1 : new_layout->slave.height);
    for (i = 0; i < dzen.slave_win.max_lines; i++) {
        LayoutRect child;
        if (horizontal_menu) {
            layout_menu_child(new_layout, i, dzen.slave_win.max_lines, &child);
            XMoveResizeWindow(dzen.dpy, dzen.slave_win.line[i], child.x, child.y, child.width, child.height);
        } else {
            XMoveResizeWindow(dzen.dpy, dzen.slave_win.line[i], 0, i * dzen.line_height, new_layout->slave.width,
                              dzen.line_height);
        }
    }
}

void windows_create(Bool use_ewmh_dock, const ResolvedLayout *layout) {
    XSetWindowAttributes wa;
    Window               root = RootWindow(dzen.dpy, dzen.screen);
    XClassHint          *class_hint;
    int                  i;

    if ((dzen.norm[ColBG] = get_color(text_buffer_data(&dzen.bg))) == ~0lu)
        eprint("dzen: error, cannot allocate color '%s'\n", text_buffer_data(&dzen.bg));
    if ((dzen.norm[ColFG] = get_color(text_buffer_data(&dzen.fg))) == ~0lu)
        eprint("dzen: error, cannot allocate color '%s'\n", text_buffer_data(&dzen.fg));

    create_gcs();

    wa.override_redirect = use_ewmh_dock ? 0 : 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask        = ExposureMask | ButtonReleaseMask | ButtonPressMask | ButtonMotionMask | EnterWindowMask |
                    LeaveWindowMask | KeyPressMask | PointerMotionMask;

    dzen.title_win.win    = XCreateWindow(dzen.dpy, root, dzen.title_win.x, dzen.title_win.y, dzen.title_win.width,
                                          dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                          DefaultVisual(dzen.dpy, dzen.screen),
                                          CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    class_hint            = XAllocClassHint();
    class_hint->res_name  = "dzen2";
    class_hint->res_class = "dzen";
    XSetClassHint(dzen.dpy, dzen.title_win.win, class_hint);
    XFree(class_hint);
    XStoreName(dzen.dpy, dzen.title_win.win, text_buffer_data(&dzen.title_win.name));

    dzen.title_win.drawable =
        XCreatePixmap(dzen.dpy, root, dzen.title_win.width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
    XFillRectangle(dzen.dpy, dzen.title_win.drawable, dzen.rgc, 0, 0, dzen.title_win.width, dzen.line_height);
    set_docking_ewmh_info(dzen.title_win.win, use_ewmh_dock);

    if (!dzen.slave_win.max_lines)
        return;

    dzen.slave_win.first_line_vis = 0;
    dzen.slave_win.last_line_vis  = 0;
    dzen.slave_win.line           = emalloc(sizeof(Window) * dzen.slave_win.max_lines);
    dzen.slave_win.drawable       = emalloc(sizeof(Drawable) * dzen.slave_win.max_lines);

    if (dzen.slave_win.ishmenu) {
        int parent_width        = layout->slave.width;
        int entry_width         = layout->menu_entry_width;
        int last_width          = layout->menu_last_width;
        dzen.slave_win.issticky = True;

        dzen.slave_win.win = XCreateWindow(dzen.dpy, root, dzen.slave_win.x, dzen.slave_win.y, parent_width,
                                           dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                           DefaultVisual(dzen.dpy, dzen.screen),
                                           CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        XStoreName(dzen.dpy, dzen.slave_win.win, text_buffer_data(&dzen.slave_win.name));

        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            dzen.slave_win.drawable[i] =
                XCreatePixmap(dzen.dpy, root, last_width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
            XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, last_width, dzen.line_height);
            dzen.slave_win.line[i] = XCreateWindow(dzen.dpy, dzen.slave_win.win, i * entry_width, 0,
                                                   i == dzen.slave_win.max_lines - 1 ? last_width : entry_width,
                                                   dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen),
                                                   CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen),
                                                   CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        }

        dzen.title_win.width = parent_width;
        dzen.slave_win.width = last_width;
    } else {
        dzen.slave_win.issticky = False;
        dzen.slave_win.win = XCreateWindow(dzen.dpy, root, dzen.slave_win.x, dzen.slave_win.y, dzen.slave_win.width,
                                           dzen.slave_win.max_lines * dzen.line_height, 0,
                                           DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                           DefaultVisual(dzen.dpy, dzen.screen),
                                           CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        XStoreName(dzen.dpy, dzen.slave_win.win, text_buffer_data(&dzen.slave_win.name));

        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            dzen.slave_win.drawable[i] = XCreatePixmap(dzen.dpy, root, dzen.slave_win.width, dzen.line_height,
                                                       DefaultDepth(dzen.dpy, dzen.screen));
            XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, dzen.slave_win.width,
                           dzen.line_height);
            dzen.slave_win.line[i] = XCreateWindow(dzen.dpy, dzen.slave_win.win, 0, i * dzen.line_height,
                                                   dzen.slave_win.width, dzen.line_height, 0,
                                                   DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                                   DefaultVisual(dzen.dpy, dzen.screen),
                                                   CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        }
    }
}

void windows_destroy(void) {
    int i;

    XFreePixmap(dzen.dpy, dzen.title_win.drawable);
    if (dzen.slave_win.max_lines) {
        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            XFreePixmap(dzen.dpy, dzen.slave_win.drawable[i]);
            XDestroyWindow(dzen.dpy, dzen.slave_win.line[i]);
        }
        free(dzen.slave_win.line);
        free(dzen.slave_win.drawable);
        XDestroyWindow(dzen.dpy, dzen.slave_win.win);
    }
    XFreeGC(dzen.dpy, dzen.gc);
    XFreeGC(dzen.dpy, dzen.rgc);
    XFreeGC(dzen.dpy, dzen.tgc);
    XFreeCursor(dzen.dpy, dzen.cursor_arrow);
    XFreeCursor(dzen.dpy, dzen.cursor_hand);
    XDestroyWindow(dzen.dpy, dzen.title_win.win);
}
