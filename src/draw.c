/*
 * (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "dzen.h"
#include "action.h"
#include "font.h"
#include "util.h"
#include "windows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif

#define ARGLEN 256

#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define LNR2WINDOW(lnr) lnr == -1 ? 0 : 1

typedef struct ICON_C {
    char   name[ARGLEN];
    Pixmap p;

    int    w, h;
} icon_c;

sens_w            window_sens[2];
static TextBuffer parse_scratch;

/* command types for the in-text parser */
enum ctype {
    bg,
    fg,
    icon,
    rect,
    titlewin,
    recto,
    circle,
    circleo,
    pos,
    abspos,
    ibg,
    fn,
    ca,
    ba,
    leftalign,
    centeralign,
    rightalign
};

struct command_lookup {
    const char *name;
    int         id;
    int         off;
};

// clang-format off
struct command_lookup cmd_lookup_table[] = {
    { "fg(",        fg,         3},
    { "bg(",        bg,         3},
    { "i(",         icon,       2},
    { "r(",         rect,       2},
    { "ro(",        recto,      3},
    { "c(",         circle,     2},
    { "co(",        circleo,    3},
    { "p(",         pos,        2},
    { "pa(",        abspos,     3},
    { "ib(",        ibg,        3},
    { "fn(",        fn,         3},
    { "ca(",        ca,         3},
    { "ba(",        ba,         3},
    { "left(",      leftalign,  5},
    { "right(",     rightalign, 6},
    { "center(",    centeralign,7},
    { "tw(",        titlewin,   3},
    { 0,            0,          0}
};
// clang-format on

int  get_tokval(const char *line, char *buf, char **retdata);
int  get_token(const char *line, char *valbuf, int *t, char **tval);

void drawtext(const char *text, int reverse, int line, int align) {
    if (!reverse) {
        XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
        XFillRectangle(dzen.dpy, dzen.slave_win.drawable[line], dzen.gc, 0, 0, dzen.w, dzen.h);
        XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
    } else {
        XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
        XFillRectangle(dzen.dpy, dzen.slave_win.drawable[line], dzen.rgc, 0, 0, dzen.w, dzen.h);
        XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
    }

    parse_line(text, line, align, reverse);
}

int get_tokval(const char *line, char *buf, char **retdata) {
    int i;

    /* Copy value into buffer until closing parenthesis */
    for (i = 0; i < ARGLEN - 1 && line[i] && line[i] != ')'; i++)
        buf[i] = line[i];

    buf[i] = '\0';

    if (i < ARGLEN && line[i] == ')') {
        *retdata = buf;
        return i + 1; /* Return position after ')' */
    }

    *retdata = NULL;
    return i;
}

int get_token(const char *line, char *valbuf, int *t, char **tval) {
    int   off = 0, next_pos = 0, i;
    char *tokval = NULL;

    if (*(line + 1) == ESC_CHAR)
        return 0;
    line++;

    *t = -1; /* Initialize to invalid token */
    for (i = 0; cmd_lookup_table[i].name; ++i) {
        off = cmd_lookup_table[i].off;
        if (!strncmp(line, cmd_lookup_table[i].name, off)) {
            /* Get the token value (text between parentheses) */
            next_pos = get_tokval(line + off, valbuf, &tokval);
            *t       = cmd_lookup_table[i].id;
            break;
        }
    }

    *tval = tokval;
    /* Return total offset including the ^ character */
    return (*t != -1) ? next_pos + off + 1 : 0;
}

static void setcolor(Drawable *pm, int x, int width, long tfg, long tbg, int reverse, int nobg) {
    if (nobg)
        return;

    XSetForeground(dzen.dpy, dzen.tgc, reverse ? tfg : tbg);
    XFillRectangle(dzen.dpy, *pm, dzen.tgc, x, 0, width, dzen.line_height);

    XSetForeground(dzen.dpy, dzen.tgc, reverse ? tbg : tfg);
    XSetBackground(dzen.dpy, dzen.tgc, reverse ? tfg : tbg);
}

/* Parser context structure to hold parsing state */
typedef struct {
    /* Position and dimensions */
    int         current_x, current_y, block_start_x;
    int         max_x, max_y;
    int         alignment_offset_x;

    /* Drawing state */
    int         nobg;
    int         pos_is_fixed;
    int         set_posy;
    int         reverse;
    int         nodraw;

    /* Colors */
    long        lastfg, lastbg;
    const char *current_fgcolor, *current_bgcolor;
    char        fgcolor[ARGLEN], bgcolor[ARGLEN];

    /* Font */
    Fnt        *current_font;
    int         font_was_set;

    /* Block alignment */
    int         block_align, block_width;

    /* Line buffer */
    char       *text_buffer;
    int         buffer_pos; /* buffer position */

    /* Drawing surfaces */
    Drawable    pm;

    /* Line info */
    int         line_number;
    int         align;

    /* Clickable areas tracking */
    int         sens_areas_start;

    /* Text parsing */
    const char *input_ptr;
    int         token;
    char        token_value_buf[ARGLEN]; /* Static buffer for token value */
    char       *token_value;

    /* For nodraw mode */
    TextBuffer *markup_free_text;
} ParseContext;

/* Process rectangle command */
static void process_rect_command(ParseContext *ctx) {
    int rectw, recth, rectx, recty;
    get_rect_vals(ctx->token_value, &rectw, &recth, &rectx, &recty);
    recth = recth > dzen.line_height ? dzen.line_height : recth;
    if (ctx->set_posy)
        ctx->current_y += recty;
    recty      = recty == 0 ? (dzen.line_height - recth) / 2 : (dzen.line_height - recth) / 2 + recty;
    ctx->max_x = MAX(ctx->max_x, ctx->current_x + rectx + rectw);
    ctx->current_x += !ctx->pos_is_fixed ? rectx : 0;
    setcolor(&ctx->pm, ctx->current_x, rectw, ctx->lastfg, ctx->lastbg, ctx->reverse, ctx->nobg);

    XFillRectangle(dzen.dpy, ctx->pm, dzen.tgc, ctx->current_x,
                   ctx->set_posy ? ctx->current_y : ((int)recty < 0 ? dzen.line_height + recty : recty), rectw, recth);

    ctx->current_x += !ctx->pos_is_fixed ? rectw : 0;
}

/* Process circle command */
static void process_circle_command(ParseContext *ctx) {
    int rectw, recth, rectx;
    rectx = get_circle_vals(ctx->token_value, &rectw, &recth);
    setcolor(&ctx->pm, ctx->current_x, rectw, ctx->lastfg, ctx->lastbg, ctx->reverse, ctx->nobg);
    XFillArc(dzen.dpy, ctx->pm, dzen.tgc, ctx->current_x,
             ctx->set_posy ? ctx->current_y : (dzen.line_height - rectw) / 2, rectw, rectw, 90 * 64,
             rectx > 1 ? recth * 64 : 64 * 360);
    ctx->max_x = MAX(ctx->max_x, ctx->current_x + rectw);
    ctx->current_x += !ctx->pos_is_fixed ? rectw : 0;
}

/* Process position command */
static void process_pos_command(ParseContext *ctx) {
    if (ctx->token_value && ctx->token_value[0]) {
        int r = 0;
        int n_posx, n_posy;
        r = get_pos_vals(ctx->token_value, &n_posx, &n_posy);
        if ((r == 1 && !ctx->set_posy))
            ctx->set_posy = 0;
        else if (r == 5) {
            switch (n_posx) {
            case LOCK_X:
                ctx->pos_is_fixed = 1;
                break;
            case UNLOCK_X:
                ctx->pos_is_fixed = 0;
                break;
            case LEFT:
                ctx->current_x = 0;
                break;
            case RIGHT:
                ctx->current_x = dzen.w;
                break;
            case CENTER:
                ctx->current_x = dzen.w / 2;
                break;
            case BOTTOM:
                ctx->set_posy  = 1;
                ctx->current_y = dzen.line_height;
                break;
            case TOP:
                ctx->set_posy  = 1;
                ctx->current_y = 0;
                break;
            }
        } else
            ctx->set_posy = 1;

        if (r != 2)
            ctx->current_x = ctx->current_x + n_posx < 0 ? 0 : ctx->current_x + n_posx;
        if (r != 1)
            ctx->current_y += n_posy;
    } else {
        ctx->set_posy = 0;
        int h;
        font_get_dimensions(NULL, NULL, &h);
        ctx->current_y = (dzen.line_height - h) / 2;
    }
    ctx->max_x = MAX(ctx->max_x, ctx->current_x);
}

/* Process background color command */
static void process_bg_command(ParseContext *ctx) {
    ctx->lastbg = (ctx->token_value && ctx->token_value[0]) ? (unsigned)get_color(ctx->token_value) : dzen.norm[ColBG];
    if (ctx->token_value && ctx->token_value[0]) {
        memcpy(ctx->bgcolor, ctx->token_value, strlen(ctx->token_value) + 1);
        ctx->current_bgcolor = ctx->bgcolor;
    } else {
        ctx->current_bgcolor = text_buffer_data(&dzen.bg);
    }
}

/* Process foreground color command */
static void process_fg_command(ParseContext *ctx) {
    ctx->lastfg = (ctx->token_value && ctx->token_value[0]) ? (unsigned)get_color(ctx->token_value) : dzen.norm[ColFG];
    if (ctx->token_value && ctx->token_value[0]) {
        memcpy(ctx->fgcolor, ctx->token_value, strlen(ctx->token_value) + 1);
        ctx->current_fgcolor = ctx->fgcolor;
    } else {
        ctx->current_fgcolor = text_buffer_data(&dzen.fg);
    }
    XSetForeground(dzen.dpy, dzen.tgc, ctx->lastfg);
}

/* Process clickable area command */
static void process_clickable_area(ParseContext *ctx) {
    sens_w *w = &window_sens[LNR2WINDOW(ctx->line_number)];

    if (ctx->token_value && ctx->token_value[0]) {
        click_a *area = &((*w).sens_areas[(*w).sens_areas_cnt]);
        if ((*w).sens_areas_cnt < MAX_CLICKABLE_AREAS) {
            get_sens_area(ctx->token_value, &(*area).button, (*area).cmd);
            (*area).start_x = ctx->current_x;
            (*area).start_y = ctx->current_y;
            (*area).end_y   = ctx->current_y;
            ctx->max_y      = ctx->current_y;
            (*area).active  = 0;
            if (ctx->line_number == -1) {
                (*area).win = dzen.title_win.win;
            } else {
                (*area).win = dzen.slave_win.line[ctx->line_number];
            }
            (*w).sens_areas_cnt++;
        }
    } else {
        // find most recent unclosed area
        int i;
        for (i = (*w).sens_areas_cnt - 1; i >= 0; i--)
            if (!(*w).sens_areas[i].active)
                break;
        if (i >= 0 && i < MAX_CLICKABLE_AREAS) {
            (*w).sens_areas[i].end_x  = ctx->current_x;
            (*w).sens_areas[i].end_y  = ctx->max_y;
            (*w).sens_areas[i].active = 1;
        }
    }
}

/* Process block alignment command */
static void process_block_align(ParseContext *ctx) {
    if (ctx->token_value && ctx->token_value[0])
        get_block_align_vals(ctx->token_value, &ctx->block_align, &ctx->block_width);
    else
        ctx->block_align = ctx->block_width = -1;
}

/* Process font command */
static void process_font_command(ParseContext *ctx) {
    if (ctx->token_value && ctx->token_value[0]) {
        ctx->current_font = font_set(ctx->token_value);
    } else {
        font_reset_to_default();
        ctx->current_font = font_get_current();
    }
    if (ctx->current_font) {
        ctx->current_y = ctx->set_posy ? ctx->current_y : (dzen.line_height - ctx->current_font->height) / 2;
    }
    ctx->font_was_set = 1;
}

/* Process absolute position command */
static void process_abspos_command(ParseContext *ctx) {
    if (ctx->token_value && ctx->token_value[0]) {
        int r = 0;
        int n_posx, n_posy;
        if ((r = get_pos_vals(ctx->token_value, &n_posx, &n_posy)) == 1 && !ctx->set_posy)
            ctx->set_posy = 0;
        else
            ctx->set_posy = 1;

        n_posx = n_posx < 0 ? n_posx * -1 : n_posx;
        if (r != 2)
            ctx->current_x = n_posx;
        if (r != 1)
            ctx->current_y = n_posy;
    } else {
        ctx->set_posy = 0;
        int h;
        font_get_dimensions(NULL, NULL, &h);
        ctx->current_y = (dzen.line_height - h) / 2;
    }
    ctx->max_x = MAX(ctx->max_x, ctx->current_x);
}

/* Process outlined circle command */
static void process_circleo_command(ParseContext *ctx) {
    int rectw, recth, rectx;
    rectx = get_circle_vals(ctx->token_value, &rectw, &recth);
    setcolor(&ctx->pm, ctx->current_x, rectw, ctx->lastfg, ctx->lastbg, ctx->reverse, ctx->nobg);
    XDrawArc(dzen.dpy, ctx->pm, dzen.tgc, ctx->current_x,
             ctx->set_posy ? ctx->current_y : (dzen.line_height - rectw) / 2, rectw, rectw, 90 * 64,
             rectx > 1 ? recth * 64 : 64 * 360);
    ctx->max_x = MAX(ctx->max_x, ctx->current_x + rectw);
    ctx->current_x += !ctx->pos_is_fixed ? rectw : 0;
}

/* Process outlined rectangle command */
static void process_recto_command(ParseContext *ctx) {
    int rectw, recth, rectx, recty;
    get_rect_vals(ctx->token_value, &rectw, &recth, &rectx, &recty);
    if (!rectw)
        return;

    recth = recth > dzen.line_height ? dzen.line_height - 2 : recth - 1;
    if (ctx->set_posy)
        ctx->current_y += recty;
    recty          = recty == 0 ? (dzen.line_height - recth) / 2 : (dzen.line_height - recth) / 2 + recty;
    ctx->max_x     = MAX(ctx->max_x, ctx->current_x + rectx + rectw);
    ctx->current_x = (rectx == 0) ? ctx->current_x : rectx + ctx->current_x;
    /* prevent from stairs effect when rounding recty */
    if (!((dzen.line_height - recth) % 2))
        recty--;
    setcolor(&ctx->pm, ctx->current_x, rectw, ctx->lastfg, ctx->lastbg, ctx->reverse, ctx->nobg);
    XDrawRectangle(dzen.dpy, ctx->pm, dzen.tgc, ctx->current_x,
                   ctx->set_posy ? ctx->current_y : ((int)recty < 0 ? dzen.line_height + recty : recty), rectw - 1,
                   recth);
    ctx->current_x += !ctx->pos_is_fixed ? rectw : 0;
}

/* Process icon command */
static void process_icon_command(ParseContext *ctx) {
    Icon *icon_obj = get_icon(ctx->token_value);
    if (icon_obj && icon_obj->pm != None) {
        int y = (ctx->set_posy
                     ? ctx->current_y
                     : (dzen.line_height >= (int)icon_obj->h ? (dzen.line_height - (int)icon_obj->h) / 2 : 0));

        setcolor(&ctx->pm, ctx->current_x, icon_obj->w, ctx->lastfg, ctx->lastbg, ctx->reverse, ctx->nobg);

        if (icon_obj->is_xbm) {
            /* 1-bit XBM => plane copy. */
            XCopyPlane(dzen.dpy, icon_obj->pm, ctx->pm, dzen.tgc, 0, 0, icon_obj->w, icon_obj->h, ctx->current_x, y, 1);
        } else {
            /* If XPM => do XCopyArea. */
            /* But now we also check if there's a mask. */
            if (icon_obj->mask_pm != None) {
                /* Setup clip mask so we only draw opaque bits. */
                XSetClipMask(dzen.dpy, dzen.tgc, icon_obj->mask_pm);
                XSetClipOrigin(dzen.dpy, dzen.tgc, ctx->current_x, y);
            }
            XCopyArea(dzen.dpy, icon_obj->pm, ctx->pm, dzen.tgc, 0, 0, icon_obj->w, icon_obj->h, ctx->current_x, y);
            /* Restore normal clipping if we set a mask. */
            if (icon_obj->mask_pm != None) {
                XSetClipMask(dzen.dpy, dzen.tgc, None);
            }
        }

        ctx->max_x = MAX(ctx->max_x, ctx->current_x + icon_obj->w);
        if (!ctx->pos_is_fixed) {
            ctx->current_x += icon_obj->w;
        }
        ctx->max_y = MAX(ctx->max_y, y + icon_obj->h);
    }
    /* else: failed to load icon; do nothing. */
}

/* Initialize parser context with default values */
static void parse_context_init(ParseContext *ctx, const char *line, int lnr, int align, int reverse, int nodraw,
                               TextBuffer *output) {
    /* Position and dimensions */
    ctx->current_x          = 0;
    ctx->current_y          = 0;
    ctx->block_start_x      = 0;
    ctx->max_x              = 0;
    ctx->max_y              = -1;
    ctx->alignment_offset_x = 0;

    /* Drawing state */
    ctx->nobg         = 0;
    ctx->pos_is_fixed = 0;
    ctx->set_posy     = 0;
    ctx->reverse      = reverse;
    ctx->nodraw       = nodraw;

    /* Colors */
    ctx->lastfg          = dzen.norm[ColFG];
    ctx->lastbg          = dzen.norm[ColBG];
    ctx->current_fgcolor = text_buffer_data(&dzen.fg);
    ctx->current_bgcolor = text_buffer_data(&dzen.bg);

    /* Font */
    ctx->current_font = NULL;
    ctx->font_was_set = 0;

    /* Block alignment */
    ctx->block_align = -1;
    ctx->block_width = -1;

    /* Line buffer */
    text_buffer_reserve(&parse_scratch, line ? strlen(line) : 0);
    text_buffer_clear(&parse_scratch);
    ctx->text_buffer = parse_scratch.data;
    ctx->buffer_pos  = 0;

    /* Drawing surfaces */
    ctx->pm = 0;

    /* Line info */
    ctx->line_number = lnr;
    ctx->align       = align;

    /* Clickable areas tracking */
    ctx->sens_areas_start = window_sens[LNR2WINDOW(lnr)].sens_areas_cnt;

    /* Text parsing */
    ctx->input_ptr          = NULL;
    ctx->token              = -1;
    ctx->token_value_buf[0] = '\0';
    ctx->token_value        = NULL;

    /* For nodraw mode */
    ctx->markup_free_text = output;
}

static void parse_line_internal(const char *line, int lnr, int align, int reverse, int nodraw, TextBuffer *output) {
    ParseContext ctx;
    parse_context_init(&ctx, line, lnr, align, reverse, nodraw, output);

    int i, next_pos = 0, h = 0, tw = 0;
    int next_align = -1;

    /* parse line and return the text without control commands */
    if (!ctx.nodraw) {
        font_get_dimensions(NULL, NULL, &h);
        ctx.current_y          = (dzen.line_height - h) / 2;
        ctx.alignment_offset_x = 0;

        if (ctx.line_number != -1) {
            ctx.pm = XCreatePixmap(dzen.dpy, RootWindow(dzen.dpy, DefaultScreen(dzen.dpy)), dzen.slave_win.width,
                                   dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
        } else {
            ctx.pm = XCreatePixmap(dzen.dpy, RootWindow(dzen.dpy, DefaultScreen(dzen.dpy)), dzen.title_win.width,
                                   dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
        }

        if (!ctx.reverse) {
            XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColBG]);
        } else {
            XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColFG]);
        }
        XFillRectangle(dzen.dpy, ctx.pm, dzen.tgc, 0, 0, dzen.w, dzen.h);

        if (!ctx.reverse) {
            XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColFG]);
        } else {
            XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColBG]);
        }

        ctx.current_font = font_get_current();

        if (ctx.line_number != -1 && (ctx.line_number + dzen.slave_win.first_line_vis >= dzen.slave_win.tcnt)) {
            XCopyArea(dzen.dpy, ctx.pm, dzen.slave_win.drawable[ctx.line_number], dzen.gc, 0, 0, ctx.current_x,
                      dzen.line_height, ctx.alignment_offset_x, 0);
            XFreePixmap(dzen.dpy, ctx.pm);
            return;
        }
    }

    ctx.input_ptr = line;
    while (1) {
        if (*ctx.input_ptr == ESC_CHAR || *ctx.input_ptr == '\0') {
            ctx.text_buffer[ctx.buffer_pos] = '\0';

            /* clear _lock_x at EOL so final width is correct */
            if (*ctx.input_ptr == '\0')
                ctx.pos_is_fixed = 0;

            if (ctx.nodraw) {
                text_buffer_append_n(ctx.markup_free_text, ctx.text_buffer, (size_t)ctx.buffer_pos);
            } else {
                if (ctx.token != -1 && ctx.token_value) {
                    switch (ctx.token) {
                    case icon:
                        process_icon_command(&ctx);
                        break;

                    case rect:
                        process_rect_command(&ctx);
                        break;

                    case recto:
                        process_recto_command(&ctx);
                        break;

                    case circle:
                        process_circle_command(&ctx);
                        break;

                    case circleo:
                        process_circleo_command(&ctx);
                        break;

                    case pos:
                        process_pos_command(&ctx);
                        break;

                    case abspos:
                        process_abspos_command(&ctx);
                        break;

                    case ibg:
                        ctx.nobg = atoi(ctx.token_value);
                        break;

                    case bg:
                        process_bg_command(&ctx);
                        break;

                    case fg:
                        process_fg_command(&ctx);
                        break;

                    case fn:
                        process_font_command(&ctx);
                        break;
                    case ca:
                        process_clickable_area(&ctx);
                        break;
                    case ba:
                        process_block_align(&ctx);
                        break;
                    }
                    /* No need to free - token_value points into line_buffer */
                }

                /* check if text is longer than window's width */
                tw = font_get_text_width(ctx.text_buffer, (unsigned int)ctx.buffer_pos);
                while ((((tw + ctx.current_x) > (dzen.w)) || (ctx.block_align != -1 && tw > ctx.block_width)) &&
                       ctx.buffer_pos > 0) {
                    ctx.text_buffer[--ctx.buffer_pos] = '\0';
                    tw = font_get_text_width(ctx.text_buffer, (unsigned int)ctx.buffer_pos);
                }

                ctx.block_start_x = ctx.current_x;

                /* draw background for block */
                if (ctx.block_align != -1 && !ctx.nobg) {
                    setcolor(&ctx.pm, ctx.current_x, ctx.block_width, ctx.lastbg, ctx.lastbg, 0, ctx.nobg);
                    XFillRectangle(dzen.dpy, ctx.pm, dzen.tgc, ctx.current_x, 0, ctx.block_width, dzen.line_height);
                }

                if (ctx.block_align == ALIGNRIGHT)
                    ctx.current_x += (ctx.block_width - tw);
                else if (ctx.block_align == ALIGNCENTER)
                    ctx.current_x += (ctx.block_width / 2) - (tw / 2);
                ctx.max_x = MAX(ctx.max_x, ctx.current_x);
                if (!ctx.nobg)
                    setcolor(&ctx.pm, ctx.current_x, tw, ctx.lastfg, ctx.lastbg, ctx.reverse, ctx.nobg);

                font_draw_text(ctx.pm, dzen.tgc, ctx.current_x, ctx.current_y, ctx.text_buffer,
                               (unsigned int)ctx.buffer_pos, ctx.reverse, ctx.current_fgcolor, ctx.current_bgcolor);

                if (ctx.current_font) {
                    ctx.max_y = MAX(ctx.max_y, ctx.current_y + ctx.current_font->height);
                }

                if (ctx.block_align == -1) {
                    if (!ctx.pos_is_fixed || *ctx.input_ptr == '\0') {
                        ctx.current_x += tw;
                        ctx.max_x = MAX(ctx.max_x, ctx.current_x);
                    }
                } else {
                    if (ctx.pos_is_fixed)
                        ctx.current_x = ctx.block_start_x;
                    else
                        ctx.current_x = ctx.block_start_x + ctx.block_width;
                    ctx.max_x = MAX(ctx.max_x, ctx.current_x);
                }
                ctx.block_align = ctx.block_width = -1;
            }

            if (*ctx.input_ptr == '\0')
                break;

            ctx.buffer_pos  = 0;
            ctx.token       = -1;
            ctx.token_value = NULL;
            next_pos        = get_token(ctx.input_ptr, ctx.token_value_buf, &ctx.token, &ctx.token_value);
            ctx.input_ptr += next_pos;
            if (ctx.token == leftalign) {
                next_align = ALIGNLEFT;
                /* No need to free - token_value points into line_buffer */
                break;
            } else if (ctx.token == centeralign) {
                next_align = ALIGNCENTER;
                /* No need to free - token_value points into line_buffer */
                break;
            } else if (ctx.token == rightalign) {
                next_align = ALIGNRIGHT;
                /* No need to free - token_value points into line_buffer */
                break;
            }

            /* ^^ escapes */
            if (next_pos == 0 && ctx.token == -1) {
                /* Double escape - print the second ^ */
                ctx.text_buffer[ctx.buffer_pos++] = *ctx.input_ptr++;
            }
            /* Continue loop - we've already advanced past the token */
            continue;
        } else {
            ctx.text_buffer[ctx.buffer_pos++] = *ctx.input_ptr;
            ctx.input_ptr++;
        }
    }

    if (!ctx.nodraw) {
        /* expand/shrink dynamically */
        if (dzen.title_win.expand && ctx.line_number == -1) {
            i = ctx.current_x;
            switch (dzen.title_win.expand) {
            case left:
                /* grow left end */
                int new_x = dzen.title_win.x_right_corner - i > dzen.title_win.x ? dzen.title_win.x_right_corner - i
                                                                                 : dzen.title_win.x;
                windows_resize_expanded_title(ctx.current_x, new_x);
                break;
            case right:
                windows_resize_expanded_title(ctx.current_x, dzen.title_win.x);
                break;
            }

        } else {
            if (ctx.align == ALIGNLEFT)
                ctx.alignment_offset_x = 0;
            if (ctx.align == ALIGNCENTER) {
                ctx.alignment_offset_x = (ctx.line_number != -1) ? (dzen.slave_win.width - ctx.current_x) / 2
                                                                 : (dzen.title_win.width - ctx.current_x) / 2;
            } else if (ctx.align == ALIGNRIGHT) {
                ctx.alignment_offset_x = (ctx.line_number != -1) ? (dzen.slave_win.width - ctx.current_x)
                                                                 : (dzen.title_win.width - ctx.current_x);
            }
        }

        XCopyArea(dzen.dpy, ctx.pm,
                  (ctx.line_number != -1 ? dzen.slave_win.drawable[ctx.line_number] : dzen.title_win.drawable), dzen.gc,
                  0, 0, ctx.max_x, dzen.line_height, ctx.alignment_offset_x, 0);
        XFreePixmap(dzen.dpy, ctx.pm);

        /* reset font to default */
        if (ctx.font_was_set)
            font_reset_to_default();
    }

    sens_w *w = &window_sens[LNR2WINDOW(ctx.line_number)];
    for (i = ctx.sens_areas_start; i < (*w).sens_areas_cnt; i++) {
        (*w).sens_areas[i].start_x += ctx.alignment_offset_x;
        (*w).sens_areas[i].end_x += ctx.alignment_offset_x;
    }

    if (!ctx.nodraw && next_align != -1) {
        /* input_ptr now points to the character after the align token */
        parse_line_internal(ctx.input_ptr, ctx.line_number, next_align, ctx.reverse, 0, NULL);
        return;
    }
}

void parse_line(const char *line, int lnr, int align, int reverse) {
    parse_line_internal(line, lnr, align, reverse, 0, NULL);
}

void parse_line_text(const char *line, TextBuffer *output) {
    text_buffer_clear(output);
    parse_line_internal(line, -1, ALIGNLEFT, 0, 1, output);
}

static int extract_between_parentheses(const char *str, char *result, size_t capacity) {
    const char *end;
    const char *start;
    size_t      length;

    if (!str)
        return 0;

    start = strchr(str, '(');
    if (!start)
        return 0;
    start++;

    end = strchr(start, ')');
    if (!end)
        return 0;

    length = (size_t)(end - start);
    if (length >= capacity)
        return 0;

    memcpy(result, start, length);
    result[length] = '\0';
    return 1;
}

static int extract_complete_command(const char *text, const char *prefix, char *result, size_t capacity) {
    const char *end;
    size_t      length;

    if (strncmp(text, prefix, strlen(prefix)) != 0)
        return 0;
    text += strlen(prefix);
    end = strchr(text, ')');
    if (end == NULL || end[1] != '\0')
        return 0;
    length = (size_t)(end - text);
    if (length >= capacity)
        return 0;
    memcpy(result, text, length);
    result[length] = '\0';
    return 1;
}

int parse_non_drawing_commands(const char *text) {
    if (!text)
        return 1;

    if (!strncmp(text, "^border(", strlen("^border("))) {
        char value[ARGLEN];

        if (extract_complete_command(text, "^border(", value, sizeof(value)))
            apply_border_spec(value);
        return 0;
    }

    if (!strncmp(text, "^togglecollapse()", strlen("^togglecollapse()"))) {
        a_togglecollapse(NULL);
        return 0;
    }
    if (!strncmp(text, "^collapse()", strlen("^collapse()"))) {
        a_collapse(NULL);
        return 0;
    }
    if (!strncmp(text, "^uncollapse()", strlen("^uncollapse()"))) {
        a_uncollapse(NULL);
        return 0;
    }

    if (!strncmp(text, "^togglestick()", strlen("^togglestick()"))) {
        a_togglestick(NULL);
        return 0;
    }
    if (!strncmp(text, "^stick()", strlen("^stick()"))) {
        a_stick(NULL);
        return 0;
    }
    if (!strncmp(text, "^unstick()", strlen("^unstick()"))) {
        a_unstick(NULL);
        return 0;
    }

    if (!strncmp(text, "^togglehide()", strlen("^togglehide()"))) {
        a_togglehide(NULL);
        return 0;
    }
    if (!strncmp(text, "^hide()", strlen("^hide()"))) {
        a_hide(NULL);
        return 0;
    }
    if (!strncmp(text, "^unhide()", strlen("^unhide()"))) {
        a_unhide(NULL);
        return 0;
    }

    if (!strncmp(text, "^raise()", strlen("^raise()"))) {
        a_raise(NULL);
        return 0;
    }

    if (!strncmp(text, "^lower()", strlen("^lower()"))) {
        a_lower(NULL);
        return 0;
    }

    if (!strncmp(text, "^scrollhome()", strlen("^scrollhome()"))) {
        a_scrollhome(NULL);
        return 0;
    }

    if (!strncmp(text, "^scrollend()", strlen("^scrollend()"))) {
        a_scrollend(NULL);
        return 0;
    }

    if (!strncmp(text, "^exit()", strlen("^exit()"))) {
        a_exit(NULL);
        return 0;
    }

    if (!strncmp(text, "^normfg(", strlen("^normfg("))) {
        char tval[ARGLEN];
        if (extract_between_parentheses(text, tval, sizeof(tval))) {
            if ((dzen.norm[ColFG] = get_color(tval)) == ~0lu)
                eprint("dzen: error, cannot allocate color '%s'\n", tval);
            text_buffer_assign(&dzen.fg, tval);
            XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
            XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
            XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
            XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
        }
        return 0;
    }

    if (!strncmp(text, "^normbg(", strlen("^normbg("))) {
        char tval[ARGLEN];
        if (extract_between_parentheses(text, tval, sizeof(tval))) {
            if ((dzen.norm[ColBG] = get_color(tval)) == ~0lu)
                eprint("dzen: error, cannot allocate color '%s'\n", tval);
            text_buffer_assign(&dzen.bg, tval);
            XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
            XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
            XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
            XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
            windows_normal_background_changed();
        }
        return 0;
    }

    if (!strncmp(text, "^normfn(", strlen("^normfn("))) {
        char tval[ARGLEN];
        if (extract_between_parentheses(text, tval, sizeof(tval))) {
            text_buffer_assign(&dzen.fnt, tval);
            font_set_default(text_buffer_data(&dzen.fnt));
        }
        return 0;
    }

    return 1;
}

static void render_header(const char *text) {
    dzen.w = dzen.title_win.width;
    dzen.h = dzen.line_height;

    window_sens[TOPWINDOW].sens_areas_cnt = 0;
    XFillRectangle(dzen.dpy, dzen.title_win.drawable, dzen.rgc, 0, 0, dzen.w, dzen.h);
    parse_line(text, -1, dzen.title_win.alignment, 0);
}

static void copy_header(void) {
    XCopyArea(dzen.dpy, dzen.title_win.drawable, dzen.title_win.win, dzen.gc, 0, 0, dzen.title_win.width,
              dzen.line_height, 0, 0);
}

void drawheader(const char *text) {
    int should_draw = parse_non_drawing_commands(text);

    if (should_draw) {
        if (text) {
            text_buffer_assign(&dzen.title_text, text);
            render_header(text_buffer_data(&dzen.title_text));
        }
    } else {
        dzen.slave_win.tcnt = -1;
        dzen.current_line   = 0;
    }

    copy_header();
}

void redrawheader(void) {
    if (dzen.title_text.data)
        render_header(text_buffer_data(&dzen.title_text));
    copy_header();
}

void drawbody(char *text) {
    char *ec;
    int   i, write_buffer = 1;

    if (dzen.slave_win.tcnt == -1) {
        dzen.slave_win.tcnt = 0;
        drawheader(text);
        return;
    }

    if ((ec = strstr(text, "^tw()")) && (ec == text || ec[-1] != '^')) {
        drawheader(ec + 5);
        return;
    }

    if (dzen.slave_win.tcnt == dzen.slave_win.tsize)
        free_buffer();

    write_buffer = parse_non_drawing_commands(text);

    if (text[0] == '^' && text[1] == 'c' && text[2] == 's') {
        free_buffer();

        for (i = 0; i < dzen.slave_win.max_lines; i++)
            XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, dzen.slave_win.width,
                           dzen.line_height);
        x_draw_body();
        return;
    }

    if (write_buffer && (dzen.slave_win.tcnt < dzen.slave_win.tsize)) {
        text_buffer_assign(&dzen.slave_win.tbuf[dzen.slave_win.tcnt], text);
        dzen.slave_win.tcnt++;
    }
}

void draw_cleanup(void) {
    text_buffer_destroy(&parse_scratch);
}
