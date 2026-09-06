# Underline and Overline Decorations

## Goal

Dzen should be able to draw a solid line along the bottom or top edge of a
formatted output span, similarly to polybar's underline and overline
formatting. A span may contain text, icons, drawing commands, and positioning
commands; decorations are not limited to text glyphs.

Underline and overline are independent: either one or both may be active for
the same span.

The feature has two layers:

- X resources and command-line options define the default thickness and color;
- in-text commands enable, customize, and disable a decoration while rendering
  an input line.

Command-line options configure the defaults only. They do not decorate all
input automatically.

## Command-line defaults

The following options define the default style:

```text
-underline THICKNESS[,COLOR]
-overline THICKNESS[,COLOR]
```

`THICKNESS` is a positive integer number of pixels. `COLOR` is any color
accepted by the existing dzen color parser, including X11 color names and
`#rrggbb` values.

Decoration specifications are strict and do not permit whitespace. For
example, `2,#ffb52a` is valid, while `2, #ffb52a`, ` 2`, and `2 ` are invalid.

When `COLOR` is omitted, the decoration follows the normal foreground color
set by `-fg`, including later `^normfg(...)` changes. An explicitly configured
color is independent of foreground changes.

If an option is absent, its built-in default is `1` pixel using the normal
foreground color. This means the parameterless in-text form is always usable:

```sh
printf '%s\n' '^underline()underlined^underline(off)' |
    dzen2 -underline 2,#ffb52a -p
```

An invalid command-line specification is a fatal startup error, consistent
with other validated geometry and style options.

## X resources

The same defaults can be configured through X resources:

```text
dzen2.underline: 2,#ffb52a
dzen2.overline:  1,#5fd7ff
```

Built-in defaults are applied first, X resources replace them, and command-line
options have the final precedence. Invalid X resource specifications are fatal
startup errors, just like invalid command-line specifications.

## In-text commands

Underline uses `^underline(ARGUMENT)` and overline uses
`^overline(ARGUMENT)`. Both commands have the same forms:

```text
^underline()                  enable with command-line defaults
^underline(COLOR)             enable with default thickness and this color
^underline(THICKNESS)         enable with this thickness and default color
^underline(THICKNESS,COLOR)   enable with both explicit values
^underline(off)               disable

^overline()                   enable with command-line defaults
^overline(COLOR)              enable with default thickness and this color
^overline(THICKNESS)          enable with this thickness and default color
^overline(THICKNESS,COLOR)    enable with both explicit values
^overline(off)                disable
```

Enabling an already active decoration closes the current span at the current
horizontal position and starts a new span with the requested style. Disabling
an inactive decoration is a no-op. Invalid arguments are silently ignored and
do not change the current state, matching existing in-text error handling.
`off` must also match exactly; surrounding whitespace is invalid.

Examples:

```text
^underline()CPU 12%^underline(off)
^overline(#5fd7ff)network^overline(off)
^underline(2,#ff5555)^overline(1,#ffffff)alert^underline(off)^overline(off)
^underline(3,#ffb52a)^r(20x8)^p(4)^c(8)^i(icon.xbm)^underline(off)
```

Decoration state starts disabled for every input line and does not leak into
later title or slave lines. `^left()`, `^center()`, and `^right()` begin a new
formatting section and therefore reset underline and overline to disabled,
just as they reset the other formatting state. A decoration left active ends
at the end of its current input line or alignment section.

## Geometry and rendering

Decorations are painted inside the existing line drawable and do not change
line height, window geometry, padding, borders, alignment, text advances, or
clickable-area coordinates.

For a line drawable of height `H` and requested thickness `T`, let the effective
thickness be `E = min(T, H)`:

- overline occupies rows `0` through `E - 1`;
- underline occupies rows `H - E` through `H - 1`.

Each decorated span starts at the current horizontal drawing position when the
decoration is enabled. Its horizontal bounds expand to cover every drawing and
movement operation until the decoration is disabled, replaced, or implicitly
closed. The following all contribute to the decorated bounds:

- literal text, including whitespace;
- icons drawn with `^i(...)`;
- filled and outlined rectangles drawn with `^r(...)` and `^ro(...)`;
- filled and outlined circles drawn with `^c(...)` and `^co(...)`;
- horizontal cursor movement produced by `^p(...)` and `^pa(...)`;
- content drawn inside clickable areas or block-alignment sections;
- future drawing commands that paint a horizontal extent.

The bounds include the actual painted width of a drawing command even when
`^p(_LOCK_X)` prevents that command from advancing the cursor. A failed command
that paints nothing and does not move the cursor adds no width. Formatting and
state-only commands such as `^fg(...)`, `^bg(...)`, `^fn(...)`, `^ib(...)`, and
`^ca(...)` do not add width by themselves, but their rendered contents do.

The final decoration is the continuous interval from the smallest to the
largest horizontal coordinate reached or painted within the span. This makes
backward and absolute positioning deterministic. It is clipped to the line
drawable, and a zero-width span draws nothing.

A decoration is painted immediately when it is disabled, replaced, or closed
at the end of an input line or alignment section. Normal painter order applies:
later content may overwrite an earlier decoration when explicit positioning
makes their areas overlap. When overline and underline are both closed
implicitly, overline is painted first and underline second.

The renderer keeps only scalar state for the currently active underline and
overline. It does not allocate or retain a list of completed spans, and it does
not parse the input twice.

Highlighting a slave-menu row changes its normal text/background colors but
does not replace explicitly selected decoration colors. Decorations that use
the normal foreground default follow the foreground used for the highlighted
render.

## Scope and validation

The first implementation must work in both Xft and core-font builds and in
title, vertical slave, and horizontal-menu line drawables. It requires:

- parser tests for every accepted form, invalid input, replacement, reset, and
  independent underline/overline state, including X resource precedence;
- visual tests for defaults, explicit styles, text, every existing graphics
  command, icons, locked and absolute positioning, alignment, menu
  highlighting, simultaneous lines, clipping, and thickness at or above the
  line height;
- documentation in `README.dzen`, followed by regeneration of `README.md` and
  `dzen2.1` when the implementation is added.

This feature does not add baseline-relative typographic underlining, dashed or
gradient lines, vertical offsets, rounded ends, or decoration-driven changes
to layout.
