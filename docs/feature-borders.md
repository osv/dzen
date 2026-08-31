# Unified Window and Configurable Borders

## Purpose

This document preserves the intent, constraints, decisions, and implementation
state for a staged refactor of dzen's title/slave window architecture. Update it
when implementation discoveries invalidate an assumption or when a user-facing
decision is made.

The feature has two related goals:

1. Present one top-level X11 window to the window manager and compositor, so a
   compositor rounds the complete notification or menu instead of independently
   rounding the title and slave windows.
2. Provide configurable, colored space on the top, right, bottom, and left of
   the content. This makes true vertical and bottom borders possible without
   drawing-language workarounds.

The work must be staged. First preserve existing behavior in integration tests,
then isolate window management, then introduce the container, and only then add
the border interface.

## Current behavior

Stage 2 presents one root child to X11 clients, window managers, and
compositors:

- `dzen outer` owns WM class/name/PID, dock state, struts, stacking, and
  application visibility.
- The unnamed title content and named slave content are children of outer.
- The slave contains one child window per visible line.

The slave is normally unmapped until `uncollapse` runs. Moving between title and
slave drives the default collapse/grab behavior. Only outer uses
override-redirect unless `-dock` is active. Content children retain the legacy
input masks, while outer deliberately has no pointer-crossing/input mask.

Horizontal menu mode differs: outer wraps the slave, the unused title remains
unmapped, and the slave contains one child per horizontal item.

`src/layout.c` resolves screen-relative title and slave rectangles. If `-tw`
and `-w` differ, the smaller/larger slave rectangle is centered relative to the
title where screen bounds permit. The first refactor slice moved creation,
destruction, drawable resizing, initial WM metadata, and layout application to
`src/windows.c`. Mapping policy, docking struts, action-driven visibility and
stacking, dynamic title expansion, and XRandR mapping preservation now also go
through that module. `main.c`, `action.c`, and `draw.c` retain policy and
rendering intent without directly mutating the existing windows.

XRandR integration case 11, **vertical menu layout**, remains the pixel
baseline. Its `11-dummy-vertical-menu.png` reference captures a centered narrow
title above a wider slave, while topology assertions now verify their absolute
geometry as children of the single outer window.

## Target window model

The target hierarchy is:

```text
root
`-- dzen outer (the only WM/compositor-facing window)
    |-- title content
    `-- slave content
        |-- visible line 0
        |-- visible line 1
        `-- ...
```

Only the outer window receives WM class, title, PID, dock type/state, and strut
properties. Content windows are implementation details. The outer window owns
the border-colored background; inset content children leave the requested
border widths visible. The compositor therefore clips and rounds one surface.

For a vertical expanded menu, the content rectangle is the union of the current
title and slave rectangles. The outer rectangle adds the four border insets.
Title and slave retain their existing relative placement, including a centered
narrow title. This definition avoids silently changing `-x`, `-y`, `-tw`, and
`-w` behavior.

When collapsed, the slave child is unmapped and the outer window is resized to
the bordered title bounds. It must not retain a blank slave-sized area. In
horizontal menu mode, the outer window wraps the slave content and the unused
title child stays unmapped.

## Border interface

Borders use one command-line option with CSS-like width expansion and an
optional final color:

```text
-b 10              top=10, right=10, bottom=10, left=10
-b 11,14           top=11, right=14, bottom=11, left=14
-b 10,red          all widths=10, explicit color=red
-b 10,8,red        top/bottom=10, right/left=8, explicit color=red
-b 10,8,10,8,red   top=10, right=8, bottom=10, left=8, explicit color=red
```

Exactly one, two, or four non-negative integer width fields are accepted. A
three-width CSS form is intentionally not accepted: with an optional color,
restricting widths to the forms above keeps parsing and error messages
unambiguous. Whitespace around comma-separated fields may be ignored, but empty
fields, negative values, trailing fields, and invalid colors are errors.

The default is zero on every side. If the color field is omitted, the border
uses the current normal background color: initially `-bg`, and subsequently the
color selected by `^normbg(...)`. This is inheritance, not a snapshot, so an
existing inherited border changes when the normal background changes. An
explicit color remains independent of later `^normbg(...)` commands.

The drawing language should expose the same grammar when the container and
relayout path are ready:

```text
^border(10)
^border(11,14)
^border(10,red)
^border(10,8,red)
^border(10,8,10,8,red)
```

`^border(...)` replaces all four widths and the color mode atomically. Omitting
the color switches back to inherited `normbg`; it does not retain an earlier
explicit border color. The update applies to the current window immediately and
causes one relayout/redraw. `^border(0)` disables visible borders. Supporting
this command is part of the planned feature unless implementation complexity or
parser ambiguity discovered during Stage 4 warrants a documented user decision.

## Geometry and behavior requirements

- Zero borders produce pixel-identical content and preserve current absolute
  title/slave geometry from the user's point of view.
- Border widths are outside content; they do not reduce `-tw`, `-w`, or line
  widths.
- The requested outer rectangle must be clamped as a unit to the selected
  Xinerama/XRandR target. The handling of borders larger than the target remains
  to be specified, but X11 dimensions must never become zero or overflow.
- `collapse`, `uncollapse`, `togglecollapse`, hide/show, raise/lower, output
  disconnect/reconnect, and horizontal menus operate on the outer window as a
  coherent application surface.
- Dock struts are published on the outer window. Whether borders contribute to
  the reserved strut is an explicit implementation decision; the recommended
  behavior is yes, because they occupy visible screen space.
- Clickable-area coordinates remain content-local. Border pixels have no action
  unless a future feature explicitly assigns one.
- Pointer transitions between title and slave must preserve existing default
  menu behavior despite both becoming children of one outer window.
- Existing title/slave names need a compatibility audit. The outer window should
  initially use the title name; relying on the slave as a separately searchable
  top-level client cannot remain compatible with the one-window goal.

## Staged implementation plan

### Stage 0: baseline (complete)

- Reuse XRandR integration case 11 with a title narrower than its centered
  slave and exact geometry assertions in addition to its screenshot.
- Run the baseline before and after the window-management extraction.

The existing XRandR case remains in place. After the outer-window refactor its
old and new screenshots must be reviewed together. Its expected image and
topology assertions may then be updated only for understood consequences of
the new hierarchy; unrelated pixel or positioning changes are regressions.

### Stage 1: isolate window management (complete)

- Window creation/destruction, drawable resizing, initial WM metadata, layout
  application, mapping, mapped-state queries, visibility, stacking, dynamic
  title expansion, docking struts, and XRandR mapping preservation are isolated
  in `src/windows.c` and `src/windows.h`.
- The X11 hierarchy and rendered pixels remain unchanged.

Before Stage 2, the remaining prerequisites are to define outer/content-local
layout rectangles, decide how the outer window represents expanded versus
collapsed geometry, and audit event routing and compatibility of the two
existing top-level window names. The Stage 0 XRandR case 11 geometry and image
remain the baseline for validating the conversion.

Stage 1 validation: `make check` passes the unit, signal, XFT visual, Xinerama,
and XRandR suites, and the core-font visual suite passes in a separate non-XFT
build. XRandR reports 104 passes and one expected rotation skip; case 11 retains
both geometries and its reference pixels.

### Stage 2: introduce the outer window (complete)

- Pure layout types resolve absolute content, expanded/collapsed outer, and
  content-local rectangles.
- Creation directly builds `root -> outer -> title/slave -> lines`; no reparent
  transition is needed.
- WM/EWMH metadata, struts, stacking, output visibility, and reconnect state
  belong to outer. The internal slave keeps `-slave-name` compatibility.
- Collapse, hide, horizontal mode, dynamic expansion, output changes, and
  pointer routing operate through the unified surface without changing content
  coordinates.
- XRandR topology and mapping tests cover the hierarchy and transitions. The
  existing case 11 and 12 golden screenshots remain pixel-identical and were
  not replaced.

Stage 3 therefore starts with a stable outer geometry/background surface. Its
`ParentRelative` background already exposes root-identical pixels in uncovered
areas around asymmetric content; static border insets can replace those areas
without another hierarchy migration.

### Stage 3: static borders (complete)

- Add border state and strict parsing for the single `-b` option.
- Allocate the border color through the existing color cache.
- Include insets in layout, outer sizing, redraw, docking, and target clamping.
- Add unit tests for the 1/2/4-value expansion, optional color, invalid input,
  geometry, and inherited-versus-explicit color state.
- Add a dedicated border visual suite, separate from the general XFT and XRandR
  cases, so the feature can be run and debugged directly. The visual runner may
  gain reusable geometry, hierarchy, mapping-state, or window-count assertions
  needed by this suite.
- Cover uniform and asymmetric widths, explicit and inherited color, collapse,
  and a centered narrow title in the dedicated suite. Expose a focused make
  target such as `make test-visual-borders`.
- Update the man page and help text.

Stage 3 adds strict, atomically replacing `-b` state, inherited or explicit
cached X11 colors, inset-aware safe layout, bordered collapse/hide/menu and
dock behavior, and XRandR state preservation.  A dedicated XFT border suite
covers pixels, exact outer/content geometry, actions, and background
inheritance.  Existing general visual golden images remain unchanged.

### Stage 4: dynamic borders

- Implement `^border(...)` with the same grammar and validation as `-b` through
  the shared border-state and relayout path.
- Test width, inherited/explicit color, collapse-state, invalid commands, and
  repeated-update behavior in the dedicated border visual suite.

## Risks and audit checklist

- Enter/leave events can differ when two root children become siblings under a
  parent; test title-to-slave and slave-to-outside transitions.
- An override-redirect parent with mapped children must preserve stacking,
  pointer grabs, keyboard grabs, and visibility semantics.
- `^p()` expansion is routed through window management but still resizes/moves
  the title content directly; Stage 2 must make it resize the outer surface too.
- XRandR reconnect state is isolated in window management but still remembers
  title and slave mapping separately; the target model needs outer visibility
  plus expanded/collapsed state.
- Pixmaps remain content-sized. Do not accidentally include border widths in
  text alignment or clickable-area calculations.
- EWMH dock struts are managed centrally but currently live on the title and are
  based on its rectangle.
- XFT and core-font builds must both compile and pass their respective visual
  suites.

## Decision log

- 2026-08-31: Use one top-level outer window; making one existing top-level
  window merely overlap another cannot give compositors one rounded surface.
- 2026-08-31: Preserve title/slave content geometry at zero border and make the
  outer window shrink on collapse.
- 2026-08-31: Use a dedicated window-management module as a behavior-preserving
  refactor before changing the hierarchy.
- 2026-08-31: XRandR case 11 is the canonical centered title/slave baseline; do
  not duplicate it in the general XFT visual suite.
- 2026-08-31: Use one `-b` option with one, two, or four CSS-like widths and an
  optional final color. Without a color, borders track the current `normbg`.
- 2026-08-31: Plan `^border(...)` with the same grammar and atomic replacement
  semantics.
- 2026-08-31: Add a dedicated border visual suite and retain XRandR case 11,
  updating its expected topology/screenshot only after old-versus-new review.
- 2026-08-31: Complete Stage 2 with a directly-created outer hierarchy and a
  `ParentRelative` outer surface. Existing XRandR menu golden images remain
  pixel-identical, so no expected images were updated.
- 2026-08-31: Complete Stage 3 with static `-b` borders. Implicit widths reserve
  horizontal insets, explicit content widths remain unchanged, oversized
  surfaces pin to the target origin and use normal X11 clipping, dock struts
  use the bordered collapsed outer, and all-zero widths retain
  `ParentRelative` even when a color was supplied. Dynamic `^border(...)`
  remains Stage 4 work.
