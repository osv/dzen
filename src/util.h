/* 
 * (C)opyright MMVI-MMVII Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVII Robert Manea <rob dot manea  at gmail dot com>
 * See LICENSE file for license details.
 */

#ifndef DZEN_UTIL_H
#define DZEN_UTIL_H

#include <stdarg.h>

/* Positioning helpers - shared between draw.c and util.c */
enum sctype { LOCK_X, UNLOCK_X, TOP, BOTTOM, CENTER, LEFT, RIGHT };

/* Memory allocation utilities */
void *emalloc(unsigned int size); /* allocates memory, exits on error */
char *estrdup(const char *str); /* duplicates str, exits on allocation error */

/* Error handling */
void eprint(const char *errstr, ...); /* prints errstr and exits with 1 */

/* Process management */
void spawn(const char *arg); /* execute arg */

/* Fast integer parsing */
/**
 * fast_parse_int - Parse integer from string without using sscanf
 * @s: String to parse
 * @result: Pointer to store the parsed integer
 * @max_digits: Maximum number of digits to parse
 * 
 * Returns: Number of characters consumed, 0 if no valid integer found
 * 
 * Example: 
 *   int val;
 *   int consumed = fast_parse_int("123abc", &val, 5);
 *   // consumed = 3, val = 123
 */
int fast_parse_int(const char *s, int *result, int max_digits);

/* Drawing command value parsers */
/**
 * get_rect_vals - Parse rectangle values: WxH+X+Y
 * @s: String containing rectangle specification
 * @w: Width output
 * @h: Height output  
 * @x: X offset output
 * @y: Y offset output
 * 
 * Returns: Number of values successfully parsed (0-4)
 * 
 * Example: "10x20+5-3" → w=10, h=20, x=5, y=-3, returns 4
 */
int get_rect_vals(char *s, int *w, int *h, int *x, int *y);

/**
 * get_circle_vals - Parse circle diameter and optional angle
 * @s: String containing circle specification
 * @d: Diameter output
 * @a: Angle output (optional)
 * 
 * Returns: Number of values parsed (1 or 2)
 * 
 * Example: "20 90" → d=20, a=90, returns 2
 */
int get_circle_vals(char *s, int *d, int *a);

/**
 * get_pos_vals - Parse position values: X;Y or special constants
 * @s: String containing position specification
 * @x: X position or special value output
 * @y: Y position output (optional)
 * 
 * Returns: 1 for X only, 2 for X;Y, 5 for special constants like _LEFT
 * 
 * Example: "10;20" → x=10, y=20, returns 2
 *          "_CENTER" → x=CENTER, returns 5
 */
int get_pos_vals(char *s, int *x, int *y);

/**
 * get_sens_area - Parse clickable area: button,command
 * @s: String containing area specification  
 * @b: Button number output
 * @cmd: Command string output (must be at least MAX_CLICKABLE_CMD_LEN bytes)
 * 
 * Returns: Always 0
 * 
 * Example: "3,echo hello" → b=3, cmd="echo hello"
 */
int get_sens_area(char *s, int *b, char *cmd);

/**
 * get_block_align_vals - Parse block alignment: width,alignment
 * @s: String containing alignment specification
 * @a: Alignment output (_LEFT, _RIGHT, _CENTER)
 * @w: Width output
 * 
 * Returns: Number of values parsed (0-2)
 * 
 * Example: "100,_CENTER" → w=100, a=ALIGNCENTER, returns 2
 */
int get_block_align_vals(char *s, int *a, int *w);

#endif /* DZEN_UTIL_H */
