#include "windows.h"

#include "dzen.h"

#include <stdlib.h>
#include <unistd.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

static Bool           outer_mapped_before_disconnect = True;
static Bool           outer_mapping_enabled          = True;
static ResolvedLayout active_layout;

static Bool           window_is_mapped(Window window, Bool *mapped) {
    XWindowAttributes attributes;

    if (!XGetWindowAttributes(dzen.dpy, window, &attributes))
        return False;
    *mapped = attributes.map_state != IsUnmapped;
    return True;
}

static LayoutRect visible_outer(Bool slave_mapped, Bool title_hidden) {
    LayoutRect result;

    if (dzen.slave_win.ishmenu) {
        result = active_layout.outer;
        if (title_hidden) {
            result.y      = active_layout.slave.y - (int)active_layout.border.top;
            result.height = active_layout.border.top + 1 + active_layout.border.bottom;
        }
    } else if (dzen.slave_win.max_lines && slave_mapped) {
        result = active_layout.outer;
    } else {
        result = active_layout.collapsed_outer;
        if (title_hidden)
            result.height = active_layout.border.top + 1 + active_layout.border.bottom;
    }
    return result;
}

static void place_surface(Bool slave_mapped, Bool title_hidden) {
    LayoutRect outer = visible_outer(slave_mapped, title_hidden);
    int        title_x;
    int        title_y;
    int        slave_x;
    int        slave_y;

    title_x = active_layout.title.x - outer.x;
    title_y = active_layout.title.y - outer.y;
    slave_x = active_layout.slave.x - outer.x;
    slave_y = active_layout.slave.y - outer.y;

    XMoveResizeWindow(dzen.dpy, dzen.outer_win, outer.x, outer.y, outer.width, outer.height);
    XMoveResizeWindow(dzen.dpy, dzen.title_win.win, title_x, title_y, active_layout.title.width,
                      title_hidden && !dzen.slave_win.ishmenu ? 1 : active_layout.title.height);
    if (dzen.slave_win.max_lines)
        XMoveResizeWindow(dzen.dpy, dzen.slave_win.win, slave_x, slave_y, active_layout.slave.width,
                          title_hidden && dzen.slave_win.ishmenu ? 1 : active_layout.slave.height);
}

static void clear_hidden_title(void) {
    XClearArea(dzen.dpy, dzen.outer_win, active_layout.title.x - active_layout.outer.x,
               active_layout.title.y - active_layout.outer.y, active_layout.title.width, active_layout.title.height,
               False);
}

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
    active_layout = *layout;
    set_layout_fields(layout, horizontal_menu, True);
}

void windows_apply_layout(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout, Bool horizontal_menu,
                          Bool title_hidden) {
    int  i;
    Bool slave_mapped = False;

    resize_drawables(old_layout, new_layout, horizontal_menu);
    active_layout = *new_layout;
    set_layout_fields(new_layout, horizontal_menu, False);
    if (dzen.slave_win.max_lines)
        windows_slave_is_mapped(&slave_mapped);
    place_surface(slave_mapped, title_hidden);
    if (!dzen.slave_win.max_lines)
        return;
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

void windows_map_title(void) {
    place_surface(False, dzen.title_win.ishidden);
    XMapWindow(dzen.dpy, dzen.title_win.win);
    if (outer_mapping_enabled)
        XMapRaised(dzen.dpy, dzen.outer_win);
    XSync(dzen.dpy, False);
}

void windows_unmap_title(void) {
    XUnmapWindow(dzen.dpy, dzen.outer_win);
}

void windows_map_slave(void) {
    int i;

    place_surface(True, dzen.title_win.ishidden);
    XMapWindow(dzen.dpy, dzen.slave_win.win);
    for (i = 0; i < dzen.slave_win.max_lines; i++)
        XMapWindow(dzen.dpy, dzen.slave_win.line[i]);
    if (dzen.title_win.ishidden && !dzen.slave_win.ishmenu) {
        XUnmapWindow(dzen.dpy, dzen.title_win.win);
        clear_hidden_title();
    }
    if (outer_mapping_enabled)
        XMapRaised(dzen.dpy, dzen.outer_win);
}

void windows_unmap_slave(void) {
    XUnmapWindow(dzen.dpy, dzen.slave_win.win);
    place_surface(False, dzen.title_win.ishidden);
    if (dzen.title_win.ishidden && !dzen.slave_win.ishmenu)
        XMapWindow(dzen.dpy, dzen.title_win.win);
}

Bool windows_slave_is_mapped(Bool *mapped) {
    return window_is_mapped(dzen.slave_win.win, mapped);
}

void windows_set_title_hidden(Bool horizontal_menu, Bool hidden) {
    Bool slave_mapped = horizontal_menu;

    if (!horizontal_menu && dzen.slave_win.max_lines)
        windows_slave_is_mapped(&slave_mapped);
    place_surface(slave_mapped, hidden);
    if (!horizontal_menu && slave_mapped) {
        if (hidden)
            XUnmapWindow(dzen.dpy, dzen.title_win.win);
        else
            XMapWindow(dzen.dpy, dzen.title_win.win);
        if (hidden)
            clear_hidden_title();
    }
}

void windows_raise_all(void) {
    XRaiseWindow(dzen.dpy, dzen.outer_win);
}

void windows_lower_all(void) {
    XLowerWindow(dzen.dpy, dzen.outer_win);
}

void windows_resize_expanded_title(int width, int x) {
    Bool slave_mapped = False;

    active_layout.title.x               = x;
    active_layout.title.width           = width;
    active_layout.title_right           = x + width;
    active_layout.collapsed_outer.x     = x - active_layout.border.left;
    active_layout.collapsed_outer.y     = active_layout.title.y - active_layout.border.top;
    active_layout.collapsed_outer.width = width + active_layout.border.left + active_layout.border.right;
    active_layout.collapsed_outer.height =
        active_layout.title.height + active_layout.border.top + active_layout.border.bottom;
    if (!dzen.slave_win.max_lines)
        active_layout.outer = active_layout.collapsed_outer;
    else if (!dzen.slave_win.ishmenu) {
        int left_edge   = x < active_layout.slave.x ? x : active_layout.slave.x;
        int right_edge  = x + width > active_layout.slave.x + active_layout.slave.width
                              ? x + width
                              : active_layout.slave.x + active_layout.slave.width;
        int top_edge    = active_layout.title.y < active_layout.slave.y ? active_layout.title.y : active_layout.slave.y;
        int bottom_edge = active_layout.title.y + active_layout.title.height >
                                  active_layout.slave.y + active_layout.slave.height
                              ? active_layout.title.y + active_layout.title.height
                              : active_layout.slave.y + active_layout.slave.height;
        active_layout.outer.x      = left_edge - active_layout.border.left;
        active_layout.outer.y      = top_edge - active_layout.border.top;
        active_layout.outer.width  = right_edge - left_edge + active_layout.border.left + active_layout.border.right;
        active_layout.outer.height = bottom_edge - top_edge + active_layout.border.top + active_layout.border.bottom;
    }
    if (dzen.slave_win.max_lines)
        windows_slave_is_mapped(&slave_mapped);
    place_surface(slave_mapped, dzen.title_win.ishidden);
}

void windows_remember_and_unmap(void) {
    window_is_mapped(dzen.outer_win, &outer_mapped_before_disconnect);
    outer_mapping_enabled = False;
    windows_unmap_title();
}

void windows_remember_slave_and_unmap(void) {
    int i;

    outer_mapped_before_disconnect = True;
    outer_mapping_enabled          = False;
    if (dzen.slave_win.ishmenu) {
        XMapWindow(dzen.dpy, dzen.slave_win.win);
        for (i = 0; i < dzen.slave_win.max_lines; i++)
            XMapWindow(dzen.dpy, dzen.slave_win.line[i]);
    } else {
        XMapWindow(dzen.dpy, dzen.title_win.win);
    }
    windows_unmap_title();
}

void windows_restore_mapping(Bool horizontal_menu) {
    (void)horizontal_menu;
    outer_mapping_enabled = True;
    if (outer_mapped_before_disconnect)
        XMapRaised(dzen.dpy, dzen.outer_win);
}

void windows_set_output_available(Bool available) {
    outer_mapping_enabled = available;
    if (!available)
        XUnmapWindow(dzen.dpy, dzen.outer_win);
}

void windows_update_docking_struts(const ResolvedLayout *layout, const XRectangle *target, const XRectangle *root,
                                   Bool dock_active) {
    Atom          partial_atom = XInternAtom(dzen.dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom          strut_atom   = XInternAtom(dzen.dpy, "_NET_WM_STRUT", False);
    Atom          cardinal     = XInternAtom(dzen.dpy, "CARDINAL", False);
    unsigned long partial[12]  = { 0 };
    unsigned long strut[4]     = { 0 };

    if (!dock_active || layout == NULL || target == NULL || root == NULL) {
        XDeleteProperty(dzen.dpy, dzen.outer_win, partial_atom);
        XDeleteProperty(dzen.dpy, dzen.outer_win, strut_atom);
        return;
    }

    if (layout->collapsed_outer.y == target->y) {
        partial[2] = layout->collapsed_outer.y + layout->collapsed_outer.height;
        partial[8] = layout->collapsed_outer.x;
        partial[9] = layout->collapsed_outer.x + layout->collapsed_outer.width - 1;
        strut[2]   = partial[2];
    } else if (layout->collapsed_outer.y + layout->collapsed_outer.height == target->y + target->height) {
        partial[3]  = root->height - layout->collapsed_outer.y;
        partial[10] = layout->collapsed_outer.x;
        partial[11] = layout->collapsed_outer.x + layout->collapsed_outer.width - 1;
        strut[3]    = partial[3];
    } else {
        XDeleteProperty(dzen.dpy, dzen.outer_win, partial_atom);
        XDeleteProperty(dzen.dpy, dzen.outer_win, strut_atom);
        return;
    }

    XChangeProperty(dzen.dpy, dzen.outer_win, partial_atom, cardinal, 32, PropModeReplace, (unsigned char *)partial,
                    12);
    XChangeProperty(dzen.dpy, dzen.outer_win, strut_atom, cardinal, 32, PropModeReplace, (unsigned char *)strut, 4);
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
    if (dzen.border.color_explicit) {
        if ((dzen.border_pixel = get_color(dzen.border.color)) == ~0lu)
            eprint("dzen: error, cannot allocate border color '%s'\n", dzen.border.color);
    } else {
        dzen.border_pixel = dzen.norm[ColBG];
    }

    create_gcs();

    wa.override_redirect = use_ewmh_dock ? 0 : 1;
    wa.event_mask        = 0;
    if (border_spec_visible(&dzen.border))
        wa.background_pixel = dzen.border_pixel;
    else
        wa.background_pixmap = ParentRelative;

    dzen.outer_win = XCreateWindow(
        dzen.dpy, root, layout->outer.x, layout->outer.y, layout->outer.width, layout->outer.height, 0,
        DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen),
        CWOverrideRedirect | (border_spec_visible(&dzen.border) ? CWBackPixel : CWBackPixmap) | CWEventMask, &wa);
    class_hint            = XAllocClassHint();
    class_hint->res_name  = "dzen2";
    class_hint->res_class = "dzen";
    XSetClassHint(dzen.dpy, dzen.outer_win, class_hint);
    XFree(class_hint);
    XStoreName(dzen.dpy, dzen.outer_win, text_buffer_data(&dzen.title_win.name));
    set_docking_ewmh_info(dzen.outer_win, use_ewmh_dock);

    wa.override_redirect = 0;
    wa.background_pixmap = ParentRelative;
    wa.event_mask        = ExposureMask | ButtonReleaseMask | ButtonPressMask | ButtonMotionMask | EnterWindowMask |
                    LeaveWindowMask | KeyPressMask | PointerMotionMask;

    dzen.title_win.win = XCreateWindow(dzen.dpy, dzen.outer_win, layout->title_local.x, layout->title_local.y,
                                       dzen.title_win.width, dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen),
                                       CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen), CWBackPixmap | CWEventMask,
                                       &wa);

    dzen.title_win.drawable =
        XCreatePixmap(dzen.dpy, root, dzen.title_win.width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
    XFillRectangle(dzen.dpy, dzen.title_win.drawable, dzen.rgc, 0, 0, dzen.title_win.width, dzen.line_height);
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

        dzen.slave_win.win = XCreateWindow(dzen.dpy, dzen.outer_win, layout->slave_local.x, layout->slave_local.y,
                                           parent_width, dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen),
                                           CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen),
                                           CWBackPixmap | CWEventMask, &wa);
        XStoreName(dzen.dpy, dzen.slave_win.win, text_buffer_data(&dzen.slave_win.name));

        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            dzen.slave_win.drawable[i] =
                XCreatePixmap(dzen.dpy, root, last_width, dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
            XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, last_width, dzen.line_height);
            dzen.slave_win.line[i] = XCreateWindow(dzen.dpy, dzen.slave_win.win, i * entry_width, 0,
                                                   i == dzen.slave_win.max_lines - 1 ? last_width : entry_width,
                                                   dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen),
                                                   CopyFromParent, DefaultVisual(dzen.dpy, dzen.screen),
                                                   CWBackPixmap | CWEventMask, &wa);
        }

        dzen.title_win.width = parent_width;
        dzen.slave_win.width = last_width;
    } else {
        dzen.slave_win.issticky = False;
        dzen.slave_win.win      = XCreateWindow(dzen.dpy, dzen.outer_win, layout->slave_local.x, layout->slave_local.y,
                                                dzen.slave_win.width, dzen.slave_win.max_lines * dzen.line_height, 0,
                                                DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                                                DefaultVisual(dzen.dpy, dzen.screen), CWBackPixmap | CWEventMask, &wa);
        XStoreName(dzen.dpy, dzen.slave_win.win, text_buffer_data(&dzen.slave_win.name));

        for (i = 0; i < dzen.slave_win.max_lines; i++) {
            dzen.slave_win.drawable[i] = XCreatePixmap(dzen.dpy, root, dzen.slave_win.width, dzen.line_height,
                                                       DefaultDepth(dzen.dpy, dzen.screen));
            XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, dzen.slave_win.width,
                           dzen.line_height);
            dzen.slave_win.line[i] =
                XCreateWindow(dzen.dpy, dzen.slave_win.win, 0, i * dzen.line_height, dzen.slave_win.width,
                              dzen.line_height, 0, DefaultDepth(dzen.dpy, dzen.screen), CopyFromParent,
                              DefaultVisual(dzen.dpy, dzen.screen), CWBackPixmap | CWEventMask, &wa);
        }
    }
}

void windows_normal_background_changed(void) {
    if (!border_spec_visible(&dzen.border) || dzen.border.color_explicit || dzen.outer_win == None)
        return;
    dzen.border_pixel = dzen.norm[ColBG];
    XSetWindowBackground(dzen.dpy, dzen.outer_win, dzen.border_pixel);
    XClearWindow(dzen.dpy, dzen.outer_win);
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
    XDestroyWindow(dzen.dpy, dzen.outer_win);
}
