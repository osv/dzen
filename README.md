    ====================================
     dzen, (c) 2007-2010 by Robert Manea
           (c) 2025 by Olexandr Sydorchuk
    ====================================

Dzen is a general purpose messaging, notification and menuing program for X11.
It was designed to be fast, tiny and scriptable in any language.

The "gadgets" subdirectory contains some tools that you can
use in combination with dzen.

Script archive with a collection of interesting ideas:
  http://gotmor.googlepages.com/dzenscriptarchive

About this dzen2 fork
=====================

Main differences between the original and this fork of `dzen2`:

* Improved performance for color/font changing (caching X11 resources).
* Increased line size from 8k to 256k.
* Align commands: `^left()`, `^center()`, `^right()`.
  You can now run a single dzen instance to render workspaces on the
  left side and other widgets on the right side.
* Improved theme changing on the fly. Allows setting default fg/bg color and font.
  See `^normfg(COLOR)`, `^normbg(COLOR)`, `^normfg(FONT)`.
* Added integration test (you can run: `make test`).
* To make assembly easier, used GNU Autotools instead of a simple Makefile.
* `-p` with argument n persist for n seconds,
  only when the mouse is not over the window (like popup or tooltip).

Features
========

* Small, fast, very tiny set of dependencies (Xlib only by default)
* Scriptable in any language
* Sophisticated formating language - including colours, icons, graphics
* Versatile - display all sorts of information (status bar, menu)
* Interactive - user defined mouse and keyboard actions. Hideable, collapsable
* Optional XFT support (enabled when xft lib present)
* Optional XINERAMA support (enabled when xinerama lib present)


Requirements
============

In order to build dzen you need the Xlib header files.


Installation
============

To enable all features and compile gadgets use next configure options:

    autoreconf -vfi
    ./configure --enable-gadgets --enable-xft --enable-xpm --enable-xinerama --enable-xcursor
    make
    make install

Note: Using the `--enable-FEATURE` options requires libraries to be installed.
      You might want to disable some feature by `--disable-FEATURE`


Contribute:
========

Feature requests, patches or anything else related to dzen can be send
to: https://github.com/osv/dzen

To update man page and README.md from README.dzen, run:

    make update-man

To run integration tests, DejaVu fonts and compilation with the `--enable-xft` options are required:

    make test


Running dzen
============

`dzen` accepts a couple of options:

    -fg     foreground color
    -bg     background color
    -fn     font
    -ta     alignment of title window content
            l(eft), c(center), r(ight)
    -tw     title window width
    -sa     alignment of slave window, see "-ta"
    -l      lines, see (1)
    -e      events and actions, see (2)
    -m      menu mode, see (3)
    -u      update contents of title and
            slave window simultaneously, see (4)
    -p      persist EOF (optional timeout in seconds)
    -x      x position
    -y      y position
    -h      line height (default: fontheight + 2 pixels)
    -w      width
    -xs     number of Xinerama screen
    -v      version information

    See "(5) In-text formating language".


X resources
===========

Dzen is able to read font and color setting from X resources.
As an example you can add following lines to ~/.Xresources

    dzen2.font:       -*-fixed-*-*-*-*-*-*-*-*-*-*-*-*
    dzen2.foreground: #22EE11
    dzen2.background: black


Window layout
=============

Dzen's window layout is as follows:

     ------------------------------------------
    |        Title window, single line         |
    `------------------------------------------´
    |                                          |
    |               scrollable                 |
    |              Slave window                |
    |             multiple lines               |
    |     lines to display simultaneously      |
    |           controlled with the            |
    |              '-l' option                 |
    |                                          |
    |                                          |
    `------------------------------------------´

The first line you provide to dzen always goes to the title window,
all other consecutive lines will be drawn to the slave window unless
you explicitly override this with the "(5) In-text formating language"
command ^tw().


QA:
---

Q1:  I don't want a slave window, what to do?
A1:  Do not provide the `-l` option, all lines will be displayed
     in the title window, this is the default behaviour.

Q2:  I used the `-l` option but no slave window appears.
A2:  With the default event/action handling the slave window will
     only be displayed if you hoover with the mouse over the title
     window. See "(2) Events and actions" if you'd like to change
     this.

Q3:  If I echo some text or cat a file dzen closes itself immediately.
A3:  There are 2 different approaches dzen uses to terminate itself,
     see next section "Termination".

Q4:  Ok, the title and slave thing works, can I update the
     contents of both windows at the same time?
A4:  Sure, see "(4) Simultaneous updates" or use the in-text
     command "^tw()" to explicitly draw to the title window.
     See "(5) In-Text formating language" for further details

Q5:  Can I change color of my input at runtime?
A5:  Yes, you can change both background and foreground colors and
     much more See "(5) In-Text formating language".

Q6:  Can I use dzen as a menu?
A6:  Yes, both vertical and horizontal menus are supported.
     See "(3) Menu" for further details.


Termination:
============

`dzen` uses two different approaches to terminate itself:

* Timed termination: if EOF is received -> terminate
  - unless the `-p` option is set
    - `-p` Without argument persist forever
    - `-p` With argument n persist for n seconds,
           only when the mouse is not over the window.

* Interactive termination: if mouse button3 is clicked -> terminate
  - this is the default behaviour, see (2)
  - in some modes the Escape key terminates too, see (2)


Return values:
--------------
0               -   dzen received EOF
1               -   some error occured, inspect the error message
user defined    -   set with 'exit:retval' action, see (2)

(1) Option `-l`: Slave window
=============================

Enables support for displaying multiple lines. The parameter to "-l"
specifies the number of lines to be displayed.

These lines of input are held in the slave window which becomes active as soon
as the pointer enters the title (default action) window.

If the mouse leaves the slave window it will be hidden unless it is set
sticky by clicking with Button2 into it (default action).

Button4 and Button5 (mouse wheel) will scroll the slave window up
and down if the content exceeds the window height (default action).



(2) Option `-e`: Events and actions
===================================

dzen allows the user to associate actions to events.

The command line syntax is as follows:

    -e 'event1=action1:option1:...option<n>,...,action<m>;...;event<l>'


Every event can take any number of actions and every action can take any number
of options. (By default limited to 64 each, easily changeable in action.h)

An example:

    -e 'button1=exec:xterm:firefox;entertitle=uncollapse,unhide;button3=exit'

Meaning:

- `button1=exec:xterm:firefox;`
On Button1 event (Button1 press on the mouse) execute xterm and
firefox.

Note: xterm and firefox are options to the exec action

- `entertitle=uncollapse,unhide;`
On entertitle (mouse pointer enters the title window) uncollapse
slave window and unhide the title window

- `button3=exit`
On button3 event exit dzen


Supported events:
-----------------

    onstart             Perform actions right after startup
    onexit              Perform actions just before exiting
    onnewinput          Perform actions if there is new input for the slave window
    button1             Mouse button1 released
    button2             Mouse button2 released
    button3             Mouse button3 released
    button4             Mouse button4 released (usually scrollwheel)
    button5             Mouse button5 released (usually scrollwheel)
    button6             Mouse button6 released
    button7             Mouse button7 released
    entertitle          Mouse enters the title window
    leavetitle          Mouse leaves the title window
    enterslave          Mouse enters the slave window
    leaveslave          Mouse leaves the slave window
    sigusr1             SIGUSR1 received
    sigusr2             SIGUSR2 received
    key_KEYNAME         Keyboard events (*)

    (*) Keyboard events:
    --------------------

    Every key can be bound to an action (see below). The format is:
    key_KEYNAME where KEYNAME is the name of the key as defined in
    keysymdef.h (usually: /usr/include/X11/keysymdef.h).  The part
    after `XK_` in keysymdef.h must be used for KEYNAME.


Supported actions:
------------------

    exec:command1:..:n  execute all given options
    menuexec            executes selected menu entry
    exit:retval         exit dzen and return 'retval'
    print:str1:...:n    write all given options to STDOUT
    menuprint           write selected menu entry to STDOUT
    collapse            collapse (roll-up) slave window
    uncollapse          uncollapse (roll-down) slave window
    togglecollapse      toggle collapsed state
    stick               stick slave window
    unstick             unstick slave window
    togglestick         toggle sticky state
    hide                hide title window
    unhide              unhide title window
    togglehide          toggle hide state
    raise               raise window to view (above all others)
    lower               lower window (behind all others)
    scrollhome          show head of input
    scrollend           show tail of input
    scrollup:n          scroll slave window n lines up   (default n=1)
    scrolldown:n        scroll slave window n lines down (default n=1)
    grabkeys            enable keyboard support
    ungrabkeys          disable keyboard support
    grabmouse           enable mouse support
                        only needed with specific windowmanagers, such as fluxbox
    ungrabmouse         release mouse
                        only needed with specific windowmanagers, such as fluxbox


    Note:   If no events/actions are specified dzen defaults to:

        Title only mode:
        ----------------

        -e 'button3=exit:13'


        Multiple lines and vertical menu mode:
        --------------------------------------

        -e 'entertitle=uncollapse,grabkeys;
            enterslave=grabkeys;leaveslave=collapse,ungrabkeys;
            button1=menuexec;button2=togglestick;button3=exit:13;
            button4=scrollup;button5=scrolldown;
            key_Escape=ungrabkeys,exit'


        Horizontal menu mode:
        ---------------------

        -e 'enterslave=grabkeys;leaveslave=ungrabkeys;
            button4=scrollup;button5=scrolldown;
            key_Left=scrollup;key_Right=scrolldown;
            button1=menuexec;button3=exit:13
            key_Escape=ungrabkeys,exit'


        If you define any events/actions, there is no default behaviour,
        i.e. you will have to specify _all_ events/actions you want to
        use.


(3) Option `-m`, Menu
=====================

Dzen provides two menu modes, vertical and horizontal menus. You can
access these modes by adding 'v'(vertical) or 'h'(horizontal) to the
'-m' option. If nothing is specified dzen defaults to vertical menus.

Vertical menu, both invocations are equivalent:

    dzen2 -p -l 4 -m < file
    dzen2 -p -l 4 -m v < file

Horizontal menu:

    dzen2 -p -l 4 -m h < file


All actions beginning with "menu" work on the selected menu entry.

Note:   Menu mode only makes sense if `-l <n>` is specified!
        Horizontal menus have no title window, so all actions
        affecting the title window will be silently discarded
        in this mode.


(4) Option `-u`, Simultaneous updates
=====================================

** DEPRECATED **

This option provides facilities to update the title and slave window at
the same time.

The way it works is best described by an example:

    Motivation:

    We want to display an updating clock in the title and some log
    output in the slave window.

    Solution:

    while true; do
          date                # output goes to the title window
          dmesg | tail -n 10  # output goes to the slave window
          sleep 1
    done | dzen2 -l 10 -u

For this to work correctly it is essential to provide exactly the number
of lines to the slave window as defined by the parameter to `-l`.


(5) In-text formating language:
==============================

This feature allows to dynamically (at runtime) format the text dzen
displays and control its behaviour.

Currently the following commands are supported:

Colors:
-------

    ^fg(color)         Set foreground color
    ^fg()              Without arguments, sets default fg color
    ^bg(color)         Set background color
    ^bg()              Without arguments, sets default bg color

Graphics:
---------

    ^i(path)           Draw icon specified by path
                       supported formats: XBM and optionally XPM

    ^r(WIDTHxHEIGHT)   Draw a rectangle with the dimensions
                       WIDTH and HEIGHT
    ^ro(WIDTHxHEIGHT)  Rectangle outline

    ^c(RADIUS)         Draw a circle with size RADIUS pixels
    ^co(RADIUS)        Circle outline

Positioning:
------------

    ^p(ARGUMENT)       Position next input amount of PIXELs to the right
                       or left of the current position
                       a.k.a. relative positioning

    ^pa(ARGUMENT)      Position next input at PIXEL
                       a.k.a. absolute positioning
                       For maximum predictability `^pa()` should only be
                       used with `-ta l` or `-sa l`

     Where ARGUMENT:

     ^p(+-X)           Move X pixels to the right or left of the current position (on the X axis)

     ^p(+-X;+-Y)       Move X pixels to the right or left and Y pixels up or down of the current
                       position (on the X and Y axis)

     ^p(;+-Y)          Move Y pixels up or down of the current position (on the Y axis)

     ^p()              Without parameters resets the Y position to its default

     ^pa()             Takes the same parameters as described above but positions at
                       the absolute X and Y coordinates

     Further ^p() also takes some symbolic names as argument:

     _LOCK_X           Lock the current X position, useful if you want to
                       align things vertically
     _UNLOCK_X         Unlock the X position
     _LEFT             Move current x-position to the left edge
     _RIGHT            Move current x-position to the right edge
     _TOP              Move current y-position to the top edge
     _CENTER           Move current x-position to center of the window
     _BOTTOM           Move current y-position to the bottom edge

    ^left()            Align next input to left. Reset settings (fg, bg, fn, etc)
    ^center()          Align next input to center. Reset settings (fg, bg, fn, etc)
    ^right()           Align next input to rigth. Reset settings (fg, bg, fn, etc)
                       Example:
                         ^left()^fg(red)Left ^center()^fg(green)Center ^right()^fg(blue)Right
                       Giving:
Left          Center          Right

Interaction:
------------

    ^ca(BTN, CMD) ... ^ca()

                       Used to define 'clickable areas' anywhere inside the
                       title window or slave window.
                       - 'BTN' denotes the mouse button (1=left, 2=right, 3=middle, etc.)
                       - 'CMD' denotes the command that should be spawned when the specific
                         area has been clicked with the defined button
                       - '...' denotes any text or formating commands dzen accepts
                       - '^ca()' without arguments denotes the end of this clickable area

                       Example:
                         foo ^ca(1, echo one)click me and i'll echo one^ca() bar

Actions as commands:
--------------------

    ^togglecollapse()
    ^collapse()
    ^uncollapse()
    ^togglestick()
    ^stick()            See section (2) "Events and actions" for a detailed description
    ^unstick()          of each command.
    ^togglehide()
    ^hide()
    ^unhide()
    ^raise()
    ^lower()
    ^scrollhome()
    ^scrollend()
    ^exit()

Other:
------

    ^tw()              draw to title window
                       This command has some annoyances, as only
                       the input after the command will be drawn
                       to the title window, so it is best used
                       only once and as first command per line.
                       Subject to be improved in the future.

    ^cs()              clear slave window
                       This command must be the first and only command
                       per line.

    ^normfg(COLOR)     Set the normal foreground color (that will be
                       used when ^bg()). You might want to use ^tw()
                       and ^cs() after. This command must be the first
                       and only command per line.

    ^normbg(COLOR)     Set the normal background color (that will be
                       used when ^bg()). You might want to use ^tw()
                       and ^cs() after. This command must be the first
                       and only command per line.

    ^normfn(FONT)      Set the normal font.

    ^ib(VALUE)         ignore background setting, VALUE can be either
                       1 to ignore or 0 to not ignore the bg color set
                       with ^bg(color).
                       This command is useful in combination with ^p()
                       and ^^pa in order to position the input inside
                       other already drawn input.

                       Example:
                         ^ib(1)^fg(red)^ro(100x15)^p(-98)^fg(blue)^r(20x10)^fg(orange)^p(3)^r(40x10)^p(4)^fg(darkgreen)^co(12)^p(2)^c(10)
                       Giving:
                         

These commands can appear anywhere and in any combination in dzen's
input.

The color can be specified either as symbolic name (e.g. red,
darkgreen, etc.) or as #rrggbb hex-value (e.g. #ffffaa).

Icons must be in the XBM or optionally XPM format, see the "bitmaps"
directory for some sample icons. With the standard "bitmap" application
you can easily draw your own icons.

Note:   Displaying XPM (pixmap) files imposes a somewhat
        higher load than lightweight XBM files, so use
        them with care in tight loops.


Note:   Doubling the `^^` character removes the special meaning from it.


Some examples:
--------------

   Input:
          ^fg(red)I'm red text ^fg(blue)I am blue

   Resulting in:
          I'm red text I am blue


   Input:
          ^bg(#ffaaaa)The ^fg(yellow)text to ^bg(blue)^fg(orange)colorize

   Resulting in:
          The text to colorize


   Input:
          ^fg(white)Some text containing ^^^^ characters

   Resulting in:
          Some text containing ^^ characters


   Input for icons:
          ^i(bitmaps/envelope.xbm) I am an envelope ^fg(yellow)and ^i(bitmaps/battery.xbm) I'm a battery.

   Resulting in:
           I am an envelope and  I'm a battery.


   Input for rectangles:
          6x4 rectangle ^r(6x4) ^fg(red)12x8 ^r(12x8) ^fg(yellow)and finally 100x15 ^r(100x15)

   Resulting in:
          6x4 rectangle  12x8  and finally 100x15 


   Input for relative positioning:
          Some text^p(100)^fg(yellow)100 pixels to the right^p(50)^fg(red)50 more pixels to the right

   Resulting in:
          Some text100 pixels to the right50 more pixels to the right


Examples:
=========

* Display message and timeout after 10 seconds:

      (echo "This is a message"; sleep 10) | dzen2 -bg darkred -fg grey85 -fn fixed


* Display message and never timeout:

      echo "This is a message"| dzen2 -p


* Display updating single line message:

      for i in $(seq 1 20); do A=${A}'='; print $A; sleep 1; done | dzen2


* Display header and a message with multiple lines:

      (echo Header; cal; sleep 20) | dzen2 -l 8

Displays "Header" in the title window and the output of cal in the
8 lines high slave window.


* Display updating messages:

      (echo Header; while true; do echo test$((i++)); sleep 1; done) | dzen2 -l 12

The slave window will update contents if new input has arrived.


* Display log files:

      (su -c "echo LOGFILENAME; tail -f /var/log/messages") | dzen2 -l 20 -x 100 -y 300 -w 500


* Monthly schedule with remind:

      (echo Monthly Schedule; remind -c1 -m) | dzen2 -l 52 -w 410 -p -fn lime -bg '#e0e8ea' -fg black -x 635


* Simple menu:

      echo "Applications" | dzen2 -l 4 -p -m < menufile


* Horizontal menu without any files:

      {echo Menu; echo -e "xterm\nxclock\nxeyes\nxfontsel"} | dzen2 -l 4 -m h -p


* Extract PIDs from the process table:

      {echo Procs; ps -a} | dzen2 -m -l 12 -p \
      -e 'button1=menuprint;button3=exit;button4=scrollup:3;button5=scrolldown:3;entertitle=uncollapse;leaveslave=collapse' \
            | awk '{print $1}'


* Dzen as xmonad (see http://xmonad.org) statusbar:

      status.sh | dzen2 -ta r -fn '-*-profont-*-*-*-*-11-*-*-*-*-*-iso8859' -bg '#aecf96' -fg black \
        -p -e 'sigusr1=raise;sigusr2=lower;onquit=exec:rm /tmp/dzen2-pid;button3=exit' & echo $! > /tmp/dzen2-pid


Have fun.
