/*
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "../config.h"
#include "font.h"
#include "border.h"
#include "text_buffer.h"
#include "util.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#endif
#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif

#define FONT     "-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*"
#define BGCOLOR  "#111111"
#define FGCOLOR  "grey70"
#define ESC_CHAR '^'

#define ALIGNCENTER 0
#define ALIGNLEFT   1
#define ALIGNRIGHT  2

#define TOPWINDOW   0
#define SLAVEWINDOW 1

#define MIN_BUF_SIZE          1024
#define MAX_CLICKABLE_AREAS   256
#define MAX_CLICKABLE_CMD_LEN 1024

#ifndef Button6
#define Button6 6
#endif

#ifndef Button7
#define Button7 7
#endif

enum { ColFG, ColBG, ColLast };

/* exapansion directions */
enum { noexpand, left, right, both };

typedef struct DZEN   Dzen;
typedef struct TW     TWIN;
typedef struct SW     SWIN;
typedef struct _Sline Sline;

typedef struct {
    Pixmap       pm;
    unsigned int w;
    unsigned int h;
    Bool         is_xbm;
    Pixmap       mask_pm;
#ifdef HAVE_XPM
    /* We keep a copy of the attributes so we can call XFreeColors + XpmFreeAttributes */
    /* Possibly track a flag to know if we actually had to allocate colormap cells */
    XpmAttributes xpma;
#endif
} Icon;

/* clickable areas */
typedef struct _CLICK_A {
    int    active;
    int    button;
    int    start_x;
    int    end_x;
    int    start_y;
    int    end_y;
    Window win; //(line)window to which the action is attached
    char   cmd[MAX_CLICKABLE_CMD_LEN];
} click_a;

typedef struct _SENS_PER_WINDOW {
    click_a sens_areas[MAX_CLICKABLE_AREAS];
    int     sens_areas_cnt;
} sens_w;

// 0: top window, 1: slave window
extern sens_w window_sens[2];

/* title window */
struct TW {
    int        x, y, width, height;

    TextBuffer name;
    Window     win;
    Drawable   drawable;
    char       alignment;
    int        expand;
    int        x_right_corner;
    Bool       ishidden;
};

/* slave window */
struct SW {
    int            x, y, width, height;

    TextBuffer     name;
    Window         win;
    Window        *line;
    Drawable      *drawable;

    /* input buffer */
    TextBuffer    *tbuf;
    int            tsize;
    int            tcnt;
    /* line fg colors */
    unsigned long *tcol;

    int            max_lines;
    int            first_line_vis;
    int            last_line_vis;
    int            sel_line;

    char           alignment;
    Bool           ismenu;
    Bool           ishmenu;
    Bool           issticky;
    Bool           ismapped;
};

struct DZEN {
    /* Window position and dimensions */
    int           x, y; /* X and Y position of the dzen window */
    int           w, h; /* Width and height of the dzen window */
    Bool          running; /* Main event loop running flag */

    /* Default colors for foreground and background */
    unsigned long norm[ColLast]; /* Array holding normal fg/bg colors */
    BorderSpec    border;
    unsigned long border_pixel;

    /* Window structures */
    Window        outer_win; /* The only root child and WM-facing application surface */
    TWIN          title_win; /* Title window (always visible, single line) */
    SWIN          slave_win; /* Slave window (optional multi-line menu) */

    /* Sensitive areas window for click handling */
    Window        sa_win; /* Window for managing clickable areas */

    /* Font and color configuration */
    TextBuffer    fnt; /* Default font name/specification */
    TextBuffer    bg; /* Default background color string */
    TextBuffer    fg; /* Default foreground color string */
    TextBuffer    title_text; /* Last rendered title, used to redraw after a resize */
    int           line_height; /* Height of each text line in pixels */

    /* X11 display and screen information */
    Display      *dpy; /* X11 display connection */
    int           screen; /* X11 screen number */
    unsigned int  depth; /* Color depth of the display */

    /* X11 graphics contexts and visual */
    Visual       *visual; /* X11 visual for rendering */
    GC            gc; /* Graphics context for normal drawing */
    GC            rgc; /* Graphics context for reverse drawing */
    GC            tgc; /* Graphics context for text drawing */

    /* Display behavior flags */
    Bool          ispersistent; /* Whether window stays visible */
    Bool          tsupdate; /* Title/slave update mode flag */
    Bool          colorize; /* Enable color processing */
    unsigned long timeout; /* Display timeout in seconds */
    long          current_line; /* Current line number for input processing */
    int           ret_val; /* Return value for exit status */

    /* Multi-monitor support (Xinerama) */
    int           xinescreen; /* Xinerama screen number (0 if no Xinerama) */

    /* Mouse cursors for different UI states */
    Cursor        cursor_arrow; /* Default arrow cursor */
    Cursor        cursor_hand; /* Hand cursor for clickable areas */
};

extern Dzen dzen;

void        free_buffer(void);
void        x_draw_body(void);

/* draw.c */
extern void drawtext(const char *text, int reverse, int line, int align);
extern void parse_line(const char *text, int linenr, int align, int reverse);
extern void parse_line_text(const char *text, TextBuffer *output);
extern void drawheader(const char *text);
extern void redrawheader(void);
extern void apply_border_spec(const char *text);
extern void drawbody(char *text);
extern void draw_cleanup(void);

/* caches.c */
long        get_color(const char *str); /* returns color of colstr */
Icon       *get_icon(const char *str);

void        init_all_caches();
void        free_all_caches();
