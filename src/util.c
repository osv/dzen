/*
 * (C)opyright MMVI-MMVII Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVII Robert Manea <rob dot manea  at gmail dot com>
 * See LICENSE file for license details.
 *
 */

#include "dzen.h"
#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ONEMASK ((size_t)(-1) / 0xFF)

void *emalloc(unsigned int size) {
    void *res = malloc(size);

    if (!res)
        eprint("fatal: could not malloc() %u bytes\n", size);
    return res;
}

void eprint(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

char *estrdup(const char *str) {
    void *res = strdup(str);

    if (!res)
        eprint("fatal: could not malloc() %u bytes\n", strlen(str));
    return res;
}
void spawn(const char *arg) {
    static const char *shell = NULL;

    if (!shell && !(shell = getenv("SHELL")))
        shell = "/bin/sh";
    if (!arg)
        return;
    /* The double-fork construct avoids zombie processes and keeps the code
     * clean from stupid signal handlers. */
    if (fork() == 0) {
        if (fork() == 0) {
            setsid();
            execl(shell, shell, "-c", arg, (char *)NULL);
            fprintf(stderr, "dzen: execl '%s -c %s'", shell, arg);
            perror(" failed");
        }
        exit(0);
    }
    wait(0);
}

int fast_parse_int(const char *s, int *result, int max_digits) {
    int val = 0;
    int i   = 0;
    int neg = 0;

    /* Handle negative numbers */
    if (*s == '-') {
        neg = 1;
        s++;
        i++;
    } else if (*s == '+') {
        /* Skip explicit positive sign */
        s++;
        i++;
    }

    /* Set unlimited digits if max_digits is 0 */
    if (max_digits == 0)
        max_digits = 10; /* int can hold at most 10 digits safely */

    /* Parse digits */
    while (i < max_digits && *s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
        i++;
    }

    /* Check if we found any digits (excluding sign) */
    if ((neg && i == 1) || (!neg && i == 0))
        return 0;

    *result = neg ? -val : val;
    return i;
}

int get_rect_vals(char *s, int *w, int *h, int *x, int *y) {
    int consumed;
    int count = 0;

    *w = *h = *x = *y = 0;

    /* Parse width */
    consumed = fast_parse_int(s, w, 5);
    if (consumed == 0)
        return 0;
    s += consumed;
    count++;

    /* Check for 'x' separator */
    if (*s == 'x') {
        s++;
        /* Parse height */
        consumed = fast_parse_int(s, h, 5);
        if (consumed) {
            s += consumed;
            count++;
        }
    }

    /* Parse optional x offset (can be + or -) */
    if (*s == '+' || *s == '-') {
        consumed = fast_parse_int(s, x, 5);
        if (consumed) {
            s += consumed;
            count++;
        }
    }

    /* Parse optional y offset */
    if (*s == '+' || *s == '-') {
        consumed = fast_parse_int(s, y, 5);
        if (consumed) {
            s += consumed;
            count++;
        }
    }

    return count;
}

int get_circle_vals(char *s, int *d, int *a) {
    int consumed;

    *d = *a = 0;

    /* Parse diameter */
    consumed = fast_parse_int(s, d, 5);
    if (consumed == 0)
        return 0;
    s += consumed;

    /* Skip whitespace */
    while (*s == ' ' || *s == '\t')
        s++;

    /* Parse optional angle */
    consumed = fast_parse_int(s, a, 5);

    return consumed ? 2 : 1;
}

int get_pos_vals(char *s, int *x, int *y) {
    int consumed;
    *x = *y = 0;

    if (s[0] == '_') {
        if (!strncmp(s, "_LOCK_X", 7)) {
            *x = LOCK_X;
        } else if (!strncmp(s, "_UNLOCK_X", 9)) {
            *x = UNLOCK_X;
        } else if (!strncmp(s, "_LEFT", 5)) {
            *x = LEFT;
        } else if (!strncmp(s, "_RIGHT", 6)) {
            *x = RIGHT;
        } else if (!strncmp(s, "_CENTER", 7)) {
            *x = CENTER;
        } else if (!strncmp(s, "_BOTTOM", 7)) {
            *x = BOTTOM;
        } else if (!strncmp(s, "_TOP", 4)) {
            *x = TOP;
        }

        return 5;
    } else {
        /* Parse X coordinate */
        consumed = fast_parse_int(s, x, 6);
        if (consumed == 0) {
            return 2; /* No valid number */
        }
        s += consumed;

        /* Check for semicolon */
        if (*s == ';') {
            s++; /* Skip semicolon */
            if (*s) {
                /* Parse Y coordinate */
                consumed = fast_parse_int(s, y, 6);
                if (consumed == 0) {
                    return 1; /* Empty after semicolon */
                }
                return 3; /* Both X and Y parsed */
            } else {
                return 1; /* Empty after semicolon */
            }
        }

        return 1; /* Only X coordinate */
    }
}

int get_sens_area(char *s, int *b, char *cmd) {
    int consumed;

    memset(cmd, 0, MAX_CLICKABLE_CMD_LEN);

    /* Parse button number */
    consumed = fast_parse_int(s, b, 5);
    if (consumed == 0) {
        *b = 1; /* Default button */
    } else {
        s += consumed;
    }

    /* Find comma and copy command */
    char *comma = strchr(s, ',');
    if (comma != NULL)
        strncpy(cmd, comma + 1, MAX_CLICKABLE_CMD_LEN - 1); /* Leave room for null terminator */

    return 0;
}

int get_block_align_vals(char *s, int *a, int *w) {
    char buf[32];
    int  consumed;

    *w = -1;
    *a = -1;

    /* Parse width */
    consumed = fast_parse_int(s, w, 10);
    if (consumed == 0)
        return 0;
    s += consumed;

    /* Check for comma */
    if (*s != ',')
        return 1;
    s++;

    /* Copy alignment string */
    int i = 0;
    while (*s && i < 31 && *s != ')' && *s != ' ') {
        buf[i++] = *s++;
    }
    buf[i] = '\0';

    /* Check alignment */
    if (!strcmp(buf, "_LEFT"))
        *a = ALIGNLEFT;
    else if (!strcmp(buf, "_RIGHT"))
        *a = ALIGNRIGHT;
    else if (!strcmp(buf, "_CENTER"))
        *a = ALIGNCENTER;
    else
        *a = -1;

    return 2;
}
