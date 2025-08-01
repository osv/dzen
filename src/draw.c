
/*
* (C)opyright 2007-2009 Robert Manea <rob dot manea at gmail dot com>
* See LICENSE file for license details.
*
*/

#include "dzen.h"
#include "action.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif

#define ARGLEN 256

#define MAX(a,b) ((a)>(b)?(a):(b))
#define LNR2WINDOW(lnr) lnr==-1?0:1

typedef struct ICON_C {
	char name[ARGLEN];
	Pixmap p;

	int w, h;
} icon_c;

int icon_cnt;
int otx;

sens_w window_sens[2];

/* command types for the in-text parser */
enum ctype  {bg, fg, icon, rect, recto, circle, circleo, pos, abspos, titlewin, ibg, fn, fixpos, ca, ba,
			 leftalign, centeralign, rightalign};

struct command_lookup {
	const char *name;
	int id;
	int off;
};

struct command_lookup cmd_lookup_table[] = {
	{ "fg(",        fg,			3},
	{ "bg(",        bg,			3},
	{ "i(",			icon,		2},
	{ "r(",	        rect,		2},
	{ "ro(",        recto,		3},
	{ "c(",	        circle,		2},
	{ "co(",        circleo,	3},
	{ "p(",	        pos,		2},
	{ "pa(",        abspos,		3},
	{ "tw(",        titlewin,	3},
	{ "ib(",        ibg,		3},
	{ "fn(",        fn,			3},
	{ "ca(",        ca,			3},
	{ "ba(",		ba,			3},
	{ "left(",		leftalign,	5},
	{ "right(",		rightalign,	6},
	{ "center(",	centeralign,7},
	{ 0,			0,			0}
};


/* positioning helpers */
enum sctype {LOCK_X, UNLOCK_X, TOP, BOTTOM, CENTER, LEFT, RIGHT};

int get_tokval(const char* line, char **retdata);
int get_token(const char*  line, int * t, char **tval);

static unsigned int
textnw(Fnt *font, const char *text, unsigned int len) {
#ifndef HAVE_XFT
	XRectangle r;

	if(font->set) {
		XmbTextExtents(font->set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(font->xfont, text, len);
#else
	XftTextExtentsUtf8(dzen.dpy, dzen.font.xftfont, (unsigned const char *) text, strlen(text), &dzen.font.extents);
	if(dzen.font.extents.height > dzen.font.height)
		dzen.font.height = dzen.font.extents.height;
	return dzen.font.extents.xOff;
#endif
}


void
drawtext(const char *text, int reverse, int line, int align) {
	if(!reverse) {
		XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
		XFillRectangle(dzen.dpy, dzen.slave_win.drawable[line], dzen.gc, 0, 0, dzen.w, dzen.h);
		XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
	}
	else {
		XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
		XFillRectangle(dzen.dpy, dzen.slave_win.drawable[line], dzen.rgc, 0, 0, dzen.w, dzen.h);
		XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
	}

	parse_line(text, line, align, reverse, 0);
}

/* Shared cache structure */
typedef struct Cache {
	char *key;
	void *value;
	struct Cache *next;
} Cache;

Cache *font_cache = NULL;
Cache *color_cache = NULL;

void* get_cached_value(Cache **cache, const char *key) {
	Cache *current = *cache;
	while (current) {
		if (strcmp(current->key, key) == 0) {
			return current->value;
		}
		current = current->next;
	}
	return NULL;
}

void add_to_cache(Cache **cache, const char *key, void *value) {
	Cache *new_entry = malloc(sizeof(Cache));
	new_entry->key = strdup(key);
	new_entry->value = value;
	new_entry->next = *cache;
	*cache = new_entry;
}

XftFont* get_cached_font(Display *display, int screen, const char *font_name) {
	XftFont *font = get_cached_value(&font_cache, font_name);
	if (!font) {
		font = XftFontOpenName(display, screen, font_name);
		if (font) {
			add_to_cache(&font_cache, font_name, font);
		}
	}
	return font;
}

void free_cache(Cache **cache) {
	Cache *current = *cache;
	while (current) {
		Cache *next = current->next;
		free(current->key);
		free(current->value);
		free(current);
		current = next;
	}
	*cache = NULL;
}

void
setfont(const char *fontstr) {
#ifndef HAVE_XFT
	char *def, **missing;
	int i, n;

	missing = NULL;
	if(dzen.font.set)
		XFreeFontSet(dzen.dpy, dzen.font.set);

	dzen.font.set = XCreateFontSet(dzen.dpy, fontstr, &missing, &n, &def);
	if(missing)
		XFreeStringList(missing);

	if(dzen.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		dzen.font.ascent = dzen.font.descent = 0;
		font_extents = XExtentsOfFontSet(dzen.font.set);
		n = XFontsOfFontSet(dzen.font.set, &xfonts, &font_names);
		for(i = 0, dzen.font.ascent = 0, dzen.font.descent = 0; i < n; i++) {
			if(dzen.font.ascent < (*xfonts)->ascent)
				dzen.font.ascent = (*xfonts)->ascent;
			if(dzen.font.descent < (*xfonts)->descent)
				dzen.font.descent = (*xfonts)->descent;
			xfonts++;
		}
	}
	else {
		if(dzen.font.xfont)
			XFreeFont(dzen.dpy, dzen.font.xfont);
		dzen.font.xfont = NULL;
		if(!(dzen.font.xfont = XLoadQueryFont(dzen.dpy, fontstr)))
			eprint("dzen: error, cannot load font: '%s'\n", fontstr);
		dzen.font.ascent = dzen.font.xfont->ascent;
		dzen.font.descent = dzen.font.xfont->descent;
	}
	dzen.font.height = dzen.font.ascent + dzen.font.descent;
#else
	if (dzen.font.xftfont)
		XftFontClose(dzen.dpy, dzen.font.xftfont);

	dzen.font.xftfont = get_cached_font(dzen.dpy, dzen.screen, fontstr);
	if (!dzen.font.xftfont)
		eprint("error, cannot load font: '%s'\n", fontstr);

	XftTextExtentsUtf8(dzen.dpy, dzen.font.xftfont, (unsigned const char *) fontstr, strlen(fontstr), &dzen.font.extents);
	dzen.font.height = dzen.font.xftfont->ascent + dzen.font.xftfont->descent;
	dzen.font.width = (dzen.font.extents.width) / strlen(fontstr);
#endif
}

int
get_tokval(const char* line, char **retdata) {
	int i;
	char tokval[ARGLEN];

	for(i=0; i < ARGLEN && (*(line+i) != ')'); i++)
		tokval[i] = *(line+i);

	tokval[i] = '\0';
	*retdata = strdup(tokval);

	return i+1;
}

int
get_token(const char *line, int * t, char **tval) {
	int off=0, next_pos=0, i;
	char *tokval = NULL;

	if(*(line+1) == ESC_CHAR)
		return 0;
	line++;

	for(i=0; cmd_lookup_table[i].name; ++i) {
		if( off=cmd_lookup_table[i].off,
				!strncmp(line, cmd_lookup_table[i].name, off) ) {
			next_pos = get_tokval(line+off, &tokval);
			*t = cmd_lookup_table[i].id;
			break;
		}
	}


	*tval = tokval;
	return next_pos+off;
}

static void
setcolor(Drawable *pm, int x, int width, long tfg, long tbg, int reverse, int nobg) {

	if(nobg)
		return;

	XSetForeground(dzen.dpy, dzen.tgc, reverse ? tfg : tbg);
	XFillRectangle(dzen.dpy, *pm, dzen.tgc, x, 0, width, dzen.line_height);

	XSetForeground(dzen.dpy, dzen.tgc, reverse ? tbg : tfg);
	XSetBackground(dzen.dpy, dzen.tgc, reverse ? tfg : tbg);
}

int 
get_sens_area(char *s, int *b, char *cmd) {
	memset(cmd, 0, 1024);
    sscanf(s, "%5d", b);
    char *comma = strchr(s, ',');
    if (comma != NULL)
        strncpy(cmd, comma+1, 1024);

	return 0;
}

static int
get_rect_vals(char *s, int *w, int *h, int *x, int *y) {
	*w=*h=*x=*y=0;

	return sscanf(s, "%5dx%5d%5d%5d", w, h, x, y);
}

static int
get_circle_vals(char *s, int *d, int *a) {
	int ret;
	*d=*a=ret=0;

	return  sscanf(s, "%5d%5d", d, a);
}

static int
get_pos_vals(char *s, int *d, int *a) {
	int i=0, ret=3, onlyx=1;
	char buf[128];
	*d=*a=0;

	if(s[0] == '_') {
		if(!strncmp(s, "_LOCK_X", 7)) {
			*d = LOCK_X;
		}
		if(!strncmp(s, "_UNLOCK_X", 8)) {
			*d = UNLOCK_X;
		}
		if(!strncmp(s, "_LEFT", 5)) {
			*d = LEFT;
		}
		if(!strncmp(s, "_RIGHT", 6)) {
			*d = RIGHT;
		}
		if(!strncmp(s, "_CENTER", 7)) {
			*d = CENTER;
		}
		if(!strncmp(s, "_BOTTOM", 7)) {
			*d = BOTTOM;
		}
		if(!strncmp(s, "_TOP", 4)) {
			*d = TOP;
		}

		return 5;
	} else {
		for(i=0; s[i] && i<128; i++) {
			if(s[i] == ';') {
				onlyx=0;
				break;
			} else
				buf[i]=s[i];
		}

		if(i) {
			buf[i]='\0';
			*d=atoi(buf);
		} else
			ret=2;

		// Check if we found a semicolon (onlyx==0 means semicolon was found)
		if(!onlyx && i < 127) {  // Ensure we have room to increment
			i++; // Move past the semicolon
			if(s[i]) {
				*a = atoi(s+i);
			} else {
				ret = 1; // Empty after semicolon
			}
		} else {
			ret = 1; // No semicolon found or at buffer limit
		}

		if(onlyx) ret=1;

		return ret;
	}
}

static int
get_block_align_vals(char *s, int *a, int *w)
{
	char buf[32];
	int r;
	*w = -1;
	r = sscanf(s, "%d,%31s", w, buf);
	if(!strcmp(buf, "_LEFT"))
		*a = ALIGNLEFT;
	else if(!strcmp(buf, "_RIGHT"))
		*a = ALIGNRIGHT;
	else if(!strcmp(buf, "_CENTER"))
		*a = ALIGNCENTER;
	else
		*a = -1;

	return r;
}

char *
parse_line(const char *line, int lnr, int align, int reverse, int nodraw) {
	/* rectangles, cirlcles*/
	int rectw, recth, rectx, recty;
	/* positioning */
	int n_posx, n_posy, set_posy=0;
	int px=0, py=0, opx=0;
	int i, next_pos=0, j=0, h=0, tw=0;
	/* buffer pos */
	const char *linep=NULL;
	/* fonts */
	int font_was_set=0;
	/* position */
	int pos_is_fixed = 0;
	/* block alignment */
	int block_align = -1;
	int block_width = -1;
	/* clickable area y tracking */
	int max_y=-1;
	/* Last max drawn pixel */
	int max_x=0;

	/* temp buffers */
	char *rbuf = NULL;
	static char *lbuf = NULL;
	/* Allocate lbuf once, if not already allocated. I dont care about free before prog terminate */
	if (lbuf == NULL) {
		lbuf = emalloc(MAX_LINE_LEN);
		lbuf[0] = '\0';
	}

	/* parser state */
	int t=-1, nobg=0;
	char *tval=NULL;

	/* X stuff */
	long lastfg = dzen.norm[ColFG], lastbg = dzen.norm[ColBG];
	Fnt *cur_fnt = NULL;
#ifndef HAVE_XFT
	XGCValues gcv;
#endif
	Drawable pm=0, bm;

#ifdef HAVE_XFT
	XftDraw *xftd=NULL;
	XftColor xftc;
	char *xftcs;
	int xftcs_f=0;
	char *xftcs_bg;
	int xftcs_bgf=0;

	xftcs    = (char *)dzen.fg;
    xftcs_bg = (char *)dzen.bg;
#endif

	/* icon cache */
	int ip;

	/* call parse_line with rest of the line and changed align: ALIGNLEFT, ALIGNCENTER, ALIGNRIGHT */
	int next_align = -1;
	/* parse_line can be called multiple times, need to change X position of sens are created in this call depending on align */
	int sens_areas_start = window_sens[LNR2WINDOW(lnr)].sens_areas_cnt;

	int xorig;

	/* parse line and return the text without control commands */
	if(nodraw) {
		rbuf = emalloc(MAX_LINE_LEN);
		rbuf[0] = '\0';
		if( (lnr + dzen.slave_win.first_line_vis) >= dzen.slave_win.tcnt)
			line = NULL;
		else
			line = dzen.slave_win.tbuf[dzen.slave_win.first_line_vis+lnr];

	}
	/* parse line and render text */
	else {
		h = dzen.font.height;
		py = (dzen.line_height - h) / 2;
		xorig = 0;
		
		if(lnr != -1) {
			pm = XCreatePixmap(dzen.dpy, RootWindow(dzen.dpy, DefaultScreen(dzen.dpy)), dzen.slave_win.width,
					dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
		}
		else {
			pm = XCreatePixmap(dzen.dpy, RootWindow(dzen.dpy, DefaultScreen(dzen.dpy)), dzen.title_win.width,
					dzen.line_height, DefaultDepth(dzen.dpy, dzen.screen));
		}

#ifdef HAVE_XFT
		xftd = XftDrawCreate(dzen.dpy, pm, DefaultVisual(dzen.dpy, dzen.screen), 
				DefaultColormap(dzen.dpy, dzen.screen));
#endif

		if(!reverse) {
			XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColBG]);
#ifdef HAVE_XFT
			xftcs_bg = (char *)dzen.bg;
			xftcs_bgf = 0;
#endif
		}
		else {
			XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColFG]);
		}
		XFillRectangle(dzen.dpy, pm, dzen.tgc, 0, 0, dzen.w, dzen.h);

		if(!reverse) {
			XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColFG]);
		}
		else {
			XSetForeground(dzen.dpy, dzen.tgc, dzen.norm[ColBG]);
		}

#ifndef HAVE_XFT
		if(!dzen.font.set){
			gcv.font = dzen.font.xfont->fid;
			XChangeGC(dzen.dpy, dzen.tgc, GCFont, &gcv);
		}
#endif
		cur_fnt = &dzen.font;

		if( lnr != -1 && (lnr + dzen.slave_win.first_line_vis >= dzen.slave_win.tcnt)) {
			XCopyArea(dzen.dpy, pm, dzen.slave_win.drawable[lnr], dzen.gc,
					0, 0, px, dzen.line_height, xorig, 0);
			XFreePixmap(dzen.dpy, pm);
			return NULL;
		}
	}

	linep = line;
	while(1) {
		if(*linep == ESC_CHAR || *linep == '\0') {
			lbuf[j] = '\0';

			/* clear _lock_x at EOL so final width is correct */
			if(*linep=='\0')
				pos_is_fixed=0;

			if(nodraw) {
				strcat(rbuf, lbuf);
			}
			else {
				if(t != -1 && tval) {
					switch(t) {
						case icon: {
							Icon* icon_obj = get_icon(tval);
							if (icon_obj && icon_obj->pm != None) {
								int y = (set_posy ? py
												  : (dzen.line_height >= (int) icon_obj->h
														 ? (dzen.line_height - (int) icon_obj->h) / 2
														 : 0));

								setcolor(&pm, px, icon_obj->w, lastfg, lastbg, reverse, nobg);

								if (icon_obj->is_xbm) {
									/* 1-bit XBM => plane copy. */
									XCopyPlane(dzen.dpy, icon_obj->pm, pm, dzen.tgc,
											   0, 0, icon_obj->w, icon_obj->h, px, y, 1);
								} else {
									/* If XPM => do XCopyArea. */
									/* But now we also check if there's a mask. */
									if (icon_obj->mask_pm != None) {
										/* Setup clip mask so we only draw opaque bits. */
										XSetClipMask(dzen.dpy, dzen.tgc, icon_obj->mask_pm);
										XSetClipOrigin(dzen.dpy, dzen.tgc, px, y);
									}
									XCopyArea(dzen.dpy, icon_obj->pm, pm, dzen.tgc,
											  0, 0, icon_obj->w, icon_obj->h, px, y);
									/* Restore normal clipping if we set a mask. */
									if (icon_obj->mask_pm != None) {
										XSetClipMask(dzen.dpy, dzen.tgc, None);
									}
								}

								max_x = MAX(max_x, px + icon_obj->w);
								if (!pos_is_fixed) {
									px += icon_obj->w;
								}
								max_y = MAX(max_y, y + icon_obj->h);
							}
							/* else: failed to load icon; do nothing. */
						} break;

						case rect:
							get_rect_vals(tval, &rectw, &recth, &rectx, &recty);
							recth = recth > dzen.line_height ? dzen.line_height : recth;
							if(set_posy)
								py += recty;
							recty =	recty == 0 ? (dzen.line_height - recth)/2 :
								(dzen.line_height - recth)/2 + recty;
							max_x = MAX(max_x, px + rectx + rectw);
							px += !pos_is_fixed ? rectx : 0;
							setcolor(&pm, px, rectw, lastfg, lastbg, reverse, nobg);

							XFillRectangle(dzen.dpy, pm, dzen.tgc, px,
									set_posy ? py :
									((int)recty < 0 ? dzen.line_height + recty : recty),
									rectw, recth);

							px += !pos_is_fixed ? rectw : 0;
							break;

						case recto:
							get_rect_vals(tval, &rectw, &recth, &rectx, &recty);
							if (!rectw) break;

							recth = recth > dzen.line_height ? dzen.line_height-2 : recth-1;
							if(set_posy)
								py += recty;
							recty =	recty == 0 ? (dzen.line_height - recth)/2 :
								(dzen.line_height - recth)/2 + recty;
							max_x = MAX(max_x, px + rectx + rectw);
							px = (rectx == 0) ? px : rectx+px;
							/* prevent from stairs effect when rounding recty */
							if (!((dzen.line_height - recth) % 2)) recty--;
							setcolor(&pm, px, rectw, lastfg, lastbg, reverse, nobg);
							XDrawRectangle(dzen.dpy, pm, dzen.tgc, px,
									set_posy ? py :
									((int)recty<0 ? dzen.line_height + recty : recty), rectw-1, recth);
							px += !pos_is_fixed ? rectw : 0;
							break;

						case circle:
							rectx = get_circle_vals(tval, &rectw, &recth);
							setcolor(&pm, px, rectw, lastfg, lastbg, reverse, nobg);
							XFillArc(dzen.dpy, pm, dzen.tgc, px, set_posy ? py :(dzen.line_height - rectw)/2,
									rectw, rectw, 90*64, rectx>1?recth*64:64*360);
							max_x = MAX(max_x, px + rectw);
							px += !pos_is_fixed ? rectw : 0;
							break;

						case circleo:
							rectx = get_circle_vals(tval, &rectw, &recth);
							setcolor(&pm, px, rectw, lastfg, lastbg, reverse, nobg);
							XDrawArc(dzen.dpy, pm, dzen.tgc, px, set_posy ? py : (dzen.line_height - rectw)/2,
									rectw, rectw, 90*64, rectx>1?recth*64:64*360);
							max_x = MAX(max_x, px + rectw);
							px += !pos_is_fixed ? rectw : 0;
							break;

						case pos:
							if(tval[0]) {
								int r=0;
								r = get_pos_vals(tval, &n_posx, &n_posy);
								if( (r == 1 && !set_posy))
									set_posy=0;
								else if (r == 5) {
									switch(n_posx) {
										case LOCK_X:
											pos_is_fixed = 1;
											break;
										case UNLOCK_X:
											pos_is_fixed = 0;
											break;
										case LEFT:
											px = 0;
											break;
										case RIGHT:
											px = dzen.w;
											break;
										case CENTER:
											px = dzen.w/2;
											break;
										case BOTTOM:
											set_posy = 1;
											py = dzen.line_height;
											break;
										case TOP:
											set_posy = 1;
											py = 0;
											break;
									}
								} else
									set_posy=1;

								if(r != 2)
									px = px+n_posx<0? 0 : px + n_posx;
								if(r != 1) 
									py += n_posy;
							} else {
								set_posy = 0;
								py = (dzen.line_height - dzen.font.height) / 2;
							}
							max_x = MAX(max_x, px);
							break;

						case abspos:
							if(tval[0]) {
								int r=0;
								if( (r=get_pos_vals(tval, &n_posx, &n_posy)) == 1 && !set_posy)
									set_posy=0;
								else
									set_posy=1;

								n_posx = n_posx < 0 ? n_posx*-1 : n_posx;
								if(r != 2)
									px = n_posx;
								if(r != 1)
									py = n_posy;
							} else {
								set_posy = 0;
								py = (dzen.line_height - dzen.font.height) / 2;
							}
							max_x = MAX(max_x, px);
							break;

						case ibg:
							nobg = atoi(tval);
							break;

						case bg:
							lastbg = tval[0] ? (unsigned)get_color(tval) : dzen.norm[ColBG];
#ifdef HAVE_XFT
							if(xftcs_bgf) free(xftcs_bg);				
							if(tval[0]) {
								xftcs_bg = estrdup(tval);
								xftcs_bgf = 1;
							} else {
								xftcs_bg = (char *)dzen.bg;
								xftcs_bgf = 0;
							}
#endif							

							break;

						case fg:
							lastfg = tval[0] ? (unsigned)get_color(tval) : dzen.norm[ColFG];
							XSetForeground(dzen.dpy, dzen.tgc, lastfg);
#ifdef HAVE_XFT
							if(tval[0]) {
								xftcs = estrdup(tval);
								xftcs_f = 1;
							} else {
								xftcs = (char *)dzen.fg;
								xftcs_f = 0;
							}
#endif							
							break;

						case fn:
							if(tval[0]) {
#ifndef HAVE_XFT
								if(!strncmp(tval, "dfnt", 4)) {
									cur_fnt = &(dzen.fnpl[atoi(tval+4)]);

									if(!cur_fnt->set) {
										gcv.font = cur_fnt->xfont->fid;
										XChangeGC(dzen.dpy, dzen.tgc, GCFont, &gcv);
									}
								}
								else
#endif					
									setfont(tval);
							}
							else {
								cur_fnt = &dzen.font;
#ifndef HAVE_XFT
								if(!cur_fnt->set){
									gcv.font = cur_fnt->xfont->fid;
									XChangeGC(dzen.dpy, dzen.tgc, GCFont, &gcv);
								}
#else
							setfont(dzen.fnt ? dzen.fnt : FONT);
#endif								
							}
							py = set_posy ? py : (dzen.line_height - cur_fnt->height) / 2;
							font_was_set = 1;
							break;
						case ca:
							; //nop to keep gcc happy
							sens_w *w = &window_sens[LNR2WINDOW(lnr)];
							
							if(tval[0]) {
								click_a *area = &((*w).sens_areas[(*w).sens_areas_cnt]);
								if((*w).sens_areas_cnt < MAX_CLICKABLE_AREAS) {
									get_sens_area(tval, 
											&(*area).button, 
											(*area).cmd);
									(*area).start_x = px;
									(*area).start_y = py;
									(*area).end_y = py;
									max_y = py;
									(*area).active = 0;
									if(lnr == -1) {
										(*area).win = dzen.title_win.win;
									} else {
										(*area).win = dzen.slave_win.line[lnr];
									}
									(*w).sens_areas_cnt++;
								}
							} else {
									//find most recent unclosed area
									for(i = (*w).sens_areas_cnt - 1; i >= 0; i--)
										if(!(*w).sens_areas[i].active)
											break;
									if(i >= 0 && i < MAX_CLICKABLE_AREAS) {
										(*w).sens_areas[i].end_x = px;
										(*w).sens_areas[i].end_y = max_y;
										(*w).sens_areas[i].active = 1;
								}
							}
							break;
						case ba:
							if(tval[0])
								get_block_align_vals(tval, &block_align, &block_width);
							else
								block_align=block_width=-1;
							break;
					}
					free(tval);
				}

				/* check if text is longer than window's width */
				tw = textnw(cur_fnt, lbuf, strlen(lbuf));
				while((((tw + px) > (dzen.w)) || (block_align!=-1 && tw>block_width)) && j>=0) {
					lbuf[--j] = '\0';
					tw = textnw(cur_fnt, lbuf, strlen(lbuf));
				}
				
				opx = px;

				/* draw background for block */
				if(block_align!=-1 && !nobg) {
					setcolor(&pm, px, rectw, lastbg, lastbg, 0, nobg);
					XFillRectangle(dzen.dpy, pm, dzen.tgc, px, 0, block_width, dzen.line_height);
				}

				if(block_align==ALIGNRIGHT)
					px += (block_width - tw);
				else if(block_align==ALIGNCENTER)
					px += (block_width/2) - (tw/2);
				max_x = MAX(max_x, px);
				if(!nobg)
					setcolor(&pm, px, tw, lastfg, lastbg, reverse, nobg);
				
#ifndef HAVE_XFT
				if(cur_fnt->set)
					XmbDrawString(dzen.dpy, pm, cur_fnt->set,
							dzen.tgc, px, py + cur_fnt->ascent, lbuf, strlen(lbuf));
				else
					XDrawString(dzen.dpy, pm, dzen.tgc, px, py+dzen.font.ascent, lbuf, strlen(lbuf));
#else
				if(reverse) {
				XftColorAllocName(dzen.dpy, DefaultVisual(dzen.dpy, dzen.screen),
						DefaultColormap(dzen.dpy, dzen.screen),  xftcs_bg,  &xftc);
				} else {
				XftColorAllocName(dzen.dpy, DefaultVisual(dzen.dpy, dzen.screen),
						DefaultColormap(dzen.dpy, dzen.screen),  xftcs,  &xftc);
				}

				XftDrawStringUtf8(xftd, &xftc, 
						cur_fnt->xftfont, px, py + dzen.font.xftfont->ascent, (const FcChar8 *)lbuf, strlen(lbuf));

				if(xftcs_f) {
					free(xftcs);
					xftcs_f = 0;
				}
				if(xftcs_bgf) {
					free(xftcs_bg);
					xftcs_bgf = 0;
				}

#endif

				max_y = MAX(max_y, py+dzen.font.height);

				if(block_align==-1) {
					if(!pos_is_fixed || *linep =='\0') {
						px += tw;
						max_x = MAX(max_x, px);
					}
				} else {
					if(pos_is_fixed)
						px = opx;
					else
						px = opx+block_width;
					max_x = MAX(max_x, px);
				}
				block_align=block_width=-1;
			}

			if(*linep=='\0')
				break;

			j=0; t=-1; tval=NULL;
			next_pos = get_token(linep, &t, &tval);
			linep += next_pos;
			if (t == leftalign) {
				next_align = ALIGNLEFT;
				break;
			} else if (t == centeralign) {
				next_align = ALIGNCENTER;
				break;
			} else if (t == rightalign) {
				next_align = ALIGNRIGHT;
				break;
			}

			/* ^^ escapes */
			if(next_pos == 0)
				lbuf[j++] = *linep++;
		}
		else
			lbuf[j++] = *linep;

		linep++;
	}

	if(!nodraw) {
		/* expand/shrink dynamically */
		if(dzen.title_win.expand && lnr == -1){
			i = px;
			switch(dzen.title_win.expand) {
				case left:
					/* grow left end */
					otx = dzen.title_win.x_right_corner - i > dzen.title_win.x ?
						dzen.title_win.x_right_corner - i : dzen.title_win.x;
					XMoveResizeWindow(dzen.dpy, dzen.title_win.win, otx, dzen.title_win.y, px, dzen.line_height);
					break;
				case right:
					XResizeWindow(dzen.dpy, dzen.title_win.win, px, dzen.line_height);
					break;
			}

		} else {
			if(align == ALIGNLEFT)
				xorig = 0;
			if(align == ALIGNCENTER) {
				xorig = (lnr != -1) ?
					(dzen.slave_win.width - px)/2 :
					(dzen.title_win.width - px)/2;
			}
			else if(align == ALIGNRIGHT) {
				xorig = (lnr != -1) ?
					(dzen.slave_win.width - px) :
					(dzen.title_win.width - px);
			}
		}


		XCopyArea(dzen.dpy, pm, (lnr != -1 ? dzen.slave_win.drawable[lnr] : dzen.title_win.drawable), dzen.gc,
				  0, 0, max_x, dzen.line_height, xorig, 0);
		XFreePixmap(dzen.dpy, pm);

		/* reset font to default */
		if(font_was_set)
			setfont(dzen.fnt ? dzen.fnt : FONT);

#ifdef HAVE_XFT
		XftDrawDestroy(xftd);
#endif
	}

	sens_w *w = &window_sens[LNR2WINDOW(lnr)];
	for(i=sens_areas_start; i<(*w).sens_areas_cnt; i++) {
		(*w).sens_areas[i].start_x += xorig;
		(*w).sens_areas[i].end_x += xorig;
	}

	if (!nodraw && next_align != -1) {
		/* linep */
		return parse_line(linep + 1, lnr, next_align, reverse, 0);
	}

	return nodraw ? rbuf : NULL;
}

char*
extract_between_parentheses(const char* str) {
    if (!str) return NULL;

    const char *start = strchr(str, '(');
    if (!start) return NULL;
    start++;

    const char *end = strchr(start, ')');
    if (!end) return NULL;

    size_t len = end - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

int
parse_non_drawing_commands(char * text) {

	if(!text)
		return 1;

	if(!strncmp(text, "^togglecollapse()", strlen("^togglecollapse()"))) {
		a_togglecollapse(NULL);
		return 0;
	}
	if(!strncmp(text, "^collapse()", strlen("^collapse()"))) {
		a_collapse(NULL);
		return 0;
	}
	if(!strncmp(text, "^uncollapse()", strlen("^uncollapse()"))) {
		a_uncollapse(NULL);
		return 0;
	}

	if(!strncmp(text, "^togglestick()", strlen("^togglestick()"))) {
		a_togglestick(NULL);
		return 0;
	}
	if(!strncmp(text, "^stick()", strlen("^stick()"))) {
		a_stick(NULL);
		return 0;
	}
	if(!strncmp(text, "^unstick()", strlen("^unstick()"))) {
		a_unstick(NULL);
		return 0;
	}

	if(!strncmp(text, "^togglehide()", strlen("^togglehide()"))) {
		a_togglehide(NULL);
		return 0;
	}
	if(!strncmp(text, "^hide()", strlen("^hide()"))) {
		a_hide(NULL);
		return 0;
	}
	if(!strncmp(text, "^unhide()", strlen("^unhide()"))) {
		a_unhide(NULL);
		return 0;
	}

	if(!strncmp(text, "^raise()", strlen("^raise()"))) {
		a_raise(NULL);
		return 0;
	}

	if(!strncmp(text, "^lower()", strlen("^lower()"))) {
		a_lower(NULL);
		return 0;
	}

	if(!strncmp(text, "^scrollhome()", strlen("^scrollhome()"))) {
		a_scrollhome(NULL);
		return 0;
	}

	if(!strncmp(text, "^scrollend()", strlen("^scrollend()"))) {
		a_scrollend(NULL);
		return 0;
	}

	if(!strncmp(text, "^exit()", strlen("^exit()"))) {
		a_exit(NULL);
		return 0;
	}

	if(!strncmp(text, "^normfg(", strlen("^normfg("))) {
		char *tval = extract_between_parentheses(text);
		if (tval) {
			if((dzen.norm[ColFG] = get_color(tval)) == ~0lu)
				eprint("dzen: error, cannot allocate color '%s'\n", tval);
			free((char *)dzen.fg);
			dzen.fg = estrdup(tval);
			XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
			XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
			XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
			XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
		}
		return 0;
	}

	if(!strncmp(text, "^normbg(", strlen("^normbg("))) {
		char *tval = extract_between_parentheses(text);
		if (tval) {
			if((dzen.norm[ColBG] = get_color(tval)) == ~0lu)
				eprint("dzen: error, cannot allocate color '%s'\n", tval);
			free((char *)dzen.bg);
			dzen.bg = estrdup(tval);
			XSetForeground(dzen.dpy, dzen.gc, dzen.norm[ColFG]);
			XSetBackground(dzen.dpy, dzen.gc, dzen.norm[ColBG]);
			XSetForeground(dzen.dpy, dzen.rgc, dzen.norm[ColBG]);
			XSetBackground(dzen.dpy, dzen.rgc, dzen.norm[ColFG]);
		}
		return 0;
	}

	if(!strncmp(text, "^normfn(", strlen("^normfn("))) {
		char *tval = extract_between_parentheses(text);
		if (tval) {
			free((char *)dzen.fnt);
			dzen.fnt = estrdup(tval);
			setfont(dzen.fnt);
		}
		return 0;
	}

	return 1;
}


void
drawheader(const char * text) {
	if(parse_non_drawing_commands((char *)text)) {
		if (text){
			dzen.w = dzen.title_win.width;
			dzen.h = dzen.line_height;
			
			window_sens[TOPWINDOW].sens_areas_cnt = 0;
			
			XFillRectangle(dzen.dpy, dzen.title_win.drawable, dzen.rgc, 0, 0, dzen.w, dzen.h);
			parse_line(text, -1, dzen.title_win.alignment, 0, 0);
		}
	} else {
		dzen.slave_win.tcnt = -1;
		dzen.cur_line = 0;
	}

	XCopyArea(dzen.dpy, dzen.title_win.drawable, dzen.title_win.win,
			dzen.gc, 0, 0, dzen.title_win.width, dzen.line_height, 0, 0);
}

void
drawbody(char * text) {
	char *ec;
	int i, write_buffer=1;
	

	if(dzen.slave_win.tcnt == -1) {
		dzen.slave_win.tcnt = 0;
		drawheader(text);
		return;
	}

	
	if((ec = strstr(text, "^tw()")) && (*(ec-1) != '^')) {
		drawheader(ec+5);
		return;
	}

	if(dzen.slave_win.tcnt == dzen.slave_win.tsize)
		free_buffer();

	write_buffer = parse_non_drawing_commands(text);


	if(text[0] == '^' && text[1] == 'c' && text[2] == 's') {
		free_buffer();

		for(i=0; i < dzen.slave_win.max_lines; i++)
			XFillRectangle(dzen.dpy, dzen.slave_win.drawable[i], dzen.rgc, 0, 0, dzen.slave_win.width, dzen.line_height);
		x_draw_body();
		return;
	}

	if( write_buffer && (dzen.slave_win.tcnt < dzen.slave_win.tsize) ) {
		dzen.slave_win.tbuf[dzen.slave_win.tcnt] = estrdup(text);
		dzen.slave_win.tcnt++;
	}
}
