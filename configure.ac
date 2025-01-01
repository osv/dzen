# -- configure.ac --

AC_PREREQ([2.69])
AC_INIT([dzen2], [1.0.0], [olexandr.syd@gmail.com])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])

AC_PROG_CC
AC_PROG_INSTALL

# ---------------------------------------------
#  Check for X11 (required); fail if missing
# ---------------------------------------------
PKG_CHECK_MODULES([X11], [x11], [],
  [AC_MSG_ERROR([*** X11 not found! Please install X11 development libraries.])])
AC_SUBST([X11_CFLAGS])
AC_SUBST([X11_LIBS])
CPPFLAGS="$CPPFLAGS $X11_CFLAGS"
LIBS="$LIBS $X11_LIBS"

# ============================================================
#  XPM
#  --enable-xpm => if not found => ERROR
#  --disable-xpm => skip check, disabled
#  default => check => if found => enable, else => disable
# ============================================================
AC_ARG_ENABLE([xpm],
  [AS_HELP_STRING([--enable-xpm],[Enable XPM (error if not found); default=auto])],
  [enable_xpm="$enableval"],
  [enable_xpm="auto"]
)

XPM_ENABLED="no"
AS_IF([test "x$enable_xpm" = "xno"], [
  # --disable-xpm => do nothing, remain disabled
], [
  # --enable-xpm or "auto"
  AC_CHECK_LIB([Xpm], [XpmReadFileToPixmap],
    [ # Found
      XPM_ENABLED="yes"
      LIBS="$LIBS -lXpm"
    ],
    [ # Not found
      AS_IF([test "x$enable_xpm" = "xyes"], [
        AC_MSG_ERROR([XPM requested (--enable-xpm) but not found.])
      ], [
        AC_MSG_WARN([XPM not found, disabling XPM support.])
      ])
    ])
])

AM_CONDITIONAL([HAVE_XPM], [test "x$XPM_ENABLED" = "xyes"])
AS_IF([test "x$XPM_ENABLED" = "xyes"], [
  AC_DEFINE([HAVE_XPM], [1], [Define if XPM is enabled])
])

# ============================================================
#  XFT
# ============================================================
AC_ARG_ENABLE([xft],
  [AS_HELP_STRING([--enable-xft],[Enable XFT (error if not found); default=auto])],
  [enable_xft="$enableval"],
  [enable_xft="auto"]
)

XFT_ENABLED="no"
AS_IF([test "x$enable_xft" = "xno"], [
  # disabled
], [
  PKG_CHECK_MODULES([XFT], [xft],
    [ # Found
      XFT_ENABLED="yes"
      CFLAGS="$CFLAGS $XFT_CFLAGS"
      LIBS="$LIBS $XFT_LIBS"
    ],
    [ # Not found
      AS_IF([test "x$enable_xft" = "xyes"], [
        AC_MSG_ERROR([XFT requested (--enable-xft) but not found.])
      ], [
        AC_MSG_WARN([XFT not found, disabling XFT support.])
      ])
    ])
])

AM_CONDITIONAL([HAVE_XFT], [test "x$XFT_ENABLED" = "xyes"])
AS_IF([test "x$XFT_ENABLED" = "xyes"], [
  AC_DEFINE([HAVE_XFT], [1], [Define if XFT is enabled])
])

# ============================================================
#  XCursor
# ============================================================
AC_ARG_ENABLE([xcursor],
  [AS_HELP_STRING([--enable-xcursor],[Enable XCursor (error if not found); default=auto])],
  [enable_xcursor="$enableval"],
  [enable_xcursor="auto"]
)

XCURSOR_ENABLED="no"
AS_IF([test "x$enable_xcursor" = "xno"], [
  # disabled
], [
  PKG_CHECK_MODULES([XCURSOR], [xcursor],
    [ # Found
      XCURSOR_ENABLED="yes"
      CFLAGS="$CFLAGS $XCURSOR_CFLAGS"
      LIBS="$LIBS $XCURSOR_LIBS"
    ],
    [ # Not found
      AS_IF([test "x$enable_xcursor" = "xyes"], [
        AC_MSG_ERROR([XCursor requested (--enable-xcursor) but not found.])
      ], [
        AC_MSG_WARN([XCursor not found, disabling XCursor support.])
      ])
    ])
])

AM_CONDITIONAL([HAVE_XCURSOR], [test "x$XCURSOR_ENABLED" = "xyes"])
AS_IF([test "x$XCURSOR_ENABLED" = "xyes"], [
  AC_DEFINE([HAVE_XCURSOR], [1], [Define if XCursor is enabled])
])

# ============================================================
#  Xinerama
# ============================================================
AC_ARG_ENABLE([xinerama],
  [AS_HELP_STRING([--enable-xinerama],[Enable Xinerama (error if not found); default=auto])],
  [enable_xinerama="$enableval"],
  [enable_xinerama="auto"]
)

XINERAMA_ENABLED="no"
AS_IF([test "x$enable_xinerama" = "xno"], [
  # disabled
], [
  PKG_CHECK_MODULES([XINERAMA], [xinerama],
    [ # Found
      XINERAMA_ENABLED="yes"
      CFLAGS="$CFLAGS $XINERAMA_CFLAGS"
      LIBS="$LIBS $XINERAMA_LIBS"
    ],
    [ # Not found
      AS_IF([test "x$enable_xinerama" = "xyes"], [
        AC_MSG_ERROR([Xinerama requested (--enable-xinerama) but not found.])
      ], [
        AC_MSG_WARN([Xinerama not found, disabling Xinerama support.])
      ])
    ])
])

AM_CONDITIONAL([HAVE_XINERAMA], [test "x$XINERAMA_ENABLED" = "xyes"])
AS_IF([test "x$XINERAMA_ENABLED" = "xyes"], [
  AC_DEFINE([HAVE_XINERAMA], [1], [Define if Xinerama is enabled])
])


# ----------------------------------------------------------
#  Gadgets
# ----------------------------------------------------------
AC_ARG_ENABLE([gadgets],
  [AS_HELP_STRING([--enable-gadgets],
  [Build dzen2 gadgets (dbar, gdbar, etc.) @<:@default=disabled@:>@])],
  [enable_gadgets="$enableval"],
  [enable_gadgets="no"]
)
AM_CONDITIONAL([ENABLE_GADGETS], [test "x$enable_gadgets" = "xyes"])
AS_IF([test "x$enable_gadgets" = "xyes"], [
  AC_DEFINE([ENABLE_GADGETS], [1], [Define if building dzen2 gadgets])
])

# ----------------------------------------------------------
#  Summary
# ----------------------------------------------------------
AC_MSG_NOTICE([
--------------------------------------------
  XPM:         $XPM_ENABLED
  XFT:         $XFT_ENABLED
  XCursor:     $XCURSOR_ENABLED
  Xinerama:    $XINERAMA_ENABLED
  Gadgets:     $enable_gadgets
--------------------------------------------
])

# ----------------------------------------------------------
#  Generate Makefiles
# ----------------------------------------------------------
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  gadgets/Makefile
])
AC_OUTPUT
