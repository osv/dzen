# Unified Window, Borders, and Padding

Dzen uses one top-level outer window so window managers and compositors see the
title, slave, padding, and border as one surface:

```text
root
`-- outer
    `-- content surface
        |-- title
        `-- slave
            `-- lines
```

The outer window owns WM metadata, stacking, dock struts, visibility, and the
border background. The permanent content surface owns the padding background.
Title and slave remain content-local children, preserving text and
clickable-area coordinates.

## Borders

`-b SPEC` accepts one, two, or four non-negative widths and an optional X11
color:

```text
-b 10
-b 10,8
-b 10,red
-b 10,8,red
-b 10,8,10,8,red
```

One width applies to all sides; two mean vertical,horizontal; four mean
top,right,bottom,left. Without a color, the border follows `-bg` and later
`^normbg(...)` changes. An explicit color is independent. `-b 0` disables the
visible border.

`^border(SPEC)` uses the same grammar and replaces the border at runtime. It
must occupy its own input line. Invalid syntax, colors, or dimensions are
silently ignored without changing the current state.

## Padding

`-pad SPEC` and the standalone runtime command `^padding(SPEC)` accept one,
two, or four non-negative widths. One width applies to all sides, two mean
vertical,horizontal, and four mean top,right,bottom,left. Runtime replacement is
atomic; `^padding(0)` disables padding, while invalid runtime input is handled
and leaves the current state unchanged.

Padding uses the normal background and follows later `^normbg(...)` changes.
The box model is `border -> padding -> content`: `-x`, `-y`, `-w`, `-tw`, and
`-h` remain content geometry, while both outer layers grow away from it. One
padding box surrounds the union of currently visible title/slave content, so no
extra gap is inserted between them. Dock struts include both padding and border.

At zero padding the content surface uses `ParentRelative`, preserving the
pre-padding rendering. Dynamic relayout and XRandR reconnect keep the same
outer, content, title, slave, and line windows, their mapping state, and their
content-local drawable coordinates.

Borders are outside explicit `-w` and `-tw` dimensions. The outer window is
clamped to the selected Xinerama/XRandR target, and dock struts include the
visible border.

## Window state

Collapsed vertical menus resize the outer window to the bordered title.
Expanded vertical menus use the union of title and slave. Horizontal menus wrap
the slave and keep the unused title unmapped.

`hide` fully unmaps title-only, collapsed vertical, and horizontal surfaces. An
expanded vertical menu keeps the slave visible. `unhide` restores the previous
state. XRandR disconnect and reconnect preserve geometry, mapping state, and the
outer window identity.

## Validation

Border and padding behavior is covered by unit, visual, Xinerama, and XRandR
tests. Run the focused tests with:

```sh
make test-border
make test-visual-borders
```

Implementation stages 1–5 are complete; padding is the latest completed stage.
