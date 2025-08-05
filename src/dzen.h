/* 
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "../config.h"
#include "font.h"
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

#define MIN_BUF_SIZE 1024
#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 262144
#endif
#define MAX_CLICKABLE_AREAS 256
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

//0: top window, 1: slave window
extern sens_w window_sens[2];

/* title window */
struct TW {
    int x, y, width, height;

    char    *name;
    Window   win;
    Drawable drawable;
    char     alignment;
    int      expand;
    int      x_right_corner;
    Bool     ishidden;
};

/* slave window */
struct SW {
    int x, y, width, height;

    char     *name;
    Window    win;
    Window   *line;
    Drawable *drawable;

    /* input buffer */
    char **tbuf;
    int    tsize;
    int    tcnt;
    /* line fg colors */
    unsigned long *tcol;

    int max_lines;
    int first_line_vis;
    int last_line_vis;
    int sel_line;

    char alignment;
    Bool ismenu;
    Bool ishmenu;
    Bool issticky;
    Bool ismapped;
};

struct DZEN {
    int           x, y, w, h;
    Bool          running;
    unsigned long norm[ColLast];

    TWIN title_win;
    SWIN slave_win;

    /* sensitive areas */
    Window sa_win;

    char *fnt;
    char *bg;
    char *fg;
    int   line_height;

    Display     *dpy;
    int          screen;
    unsigned int depth;

    Visual *visual;
    GC      gc, rgc, tgc;

    Bool          ispersistent;
    Bool          tsupdate;
    Bool          colorize;
    unsigned long timeout;
    long          cur_line;
    int           ret_val;

    /* should always be 0 if HAVE_XINERAMA not defined */
    int xinescreen;

    Cursor cursor_arrow;
    Cursor cursor_hand;
};

extern Dzen dzen;

void free_buffer(void);
void x_draw_body(void);

/* draw.c */
extern void  drawtext(const char *text, int reverse, int line, int align);
extern char *parse_line(const char *text, int linenr, int align, int reverse, int nodraw);
extern void  drawheader(const char *text);
extern void  drawbody(char *text);

/* caches.c */
long  get_color(const char *str); /* returns color of colstr */
Icon *get_icon(const char *str);

void init_all_caches();
void free_all_caches();
