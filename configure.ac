# -- configure.ac --

# Require Autoconf 2.69 or later
AC_PREREQ([2.69])

AC_INIT([dzen2], [1.0.0], [olexandr.syd@gmail.com])

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])

# Checks for programs
AC_PROG_CC     # for C compiler detection
AC_PROG_INSTALL

# -----------------------------
# Options
# -----------------------------

# 1) Whether to build the gadgets
AC_ARG_ENABLE([gadgets],
  [AS_HELP_STRING([--enable-gadgets],
  [Build dzen2 gadgets (dbar, gdbar, etc.) @<:@default=disabled@:>@])],
  [enable_gadgets="$enableval"],
  [enable_gadgets="no"]
)
AM_CONDITIONAL([ENABLE_GADGETS], [test "x$enable_gadgets" = "xyes"])

# 2) Whether to enable or disable XPM
AC_ARG_ENABLE([xpm],
  [AS_HELP_STRING([--disable-xpm],
  [Disable XPM support (default is auto-detect if available)])],
  [enable_xpm="$enableval"],
  [enable_xpm="yes"]
)

# 3) Whether to enable or disable XFT
AC_ARG_ENABLE([xft],
  [AS_HELP_STRING([--disable-xft],
  [Disable XFT support (default is auto-detect if available)])],
  [enable_xft="$enableval"],
  [enable_xft="yes"]
)

# 4) Whether to enable or disable XCursor
AC_ARG_ENABLE([xcursor],
  [AS_HELP_STRING([--disable-xcursor],
  [Disable XCursor support (default is auto-detect if available)])],
  [enable_xcursor="$enableval"],
  [enable_xcursor="yes"]
)

# 5) Whether to enable or disable Xinerama
AC_ARG_ENABLE([xinerama],
  [AS_HELP_STRING([--disable-xinerama],
  [Disable Xinerama support (default is auto-detect if available)])],
  [enable_xinerama="$enableval"],
  [enable_xinerama="yes"]
)

# -----------------------------
# Library checks
# -----------------------------
# For X11 itself:
PKG_CHECK_MODULES([X11], [x11])
# This sets variables like $X_CFLAGS, $X_LIBS, $X_PRE_LIBS
CPPFLAGS="$CPPFLAGS $X11_CFLAGS"
LIBS="$LIBS $X11_LIBS"

dnl # By default, put all found libs in LIBS/CFLAGS.  Then optionally detect xpm/xft/etc.
dnl CPPFLAGS="$CPPFLAGS $X_CFLAGS"
dnl LIBS="$LIBS $X_LIBS"

# Check for XPM if enable_xpm != no
HAVE_XPM="no"
if test "x$enable_xpm" != "xno"; then
  AC_CHECK_LIB([Xpm], [XpmCreateImageFromData],
    [HAVE_XPM="yes"; LIBS="$LIBS -lXpm"],
    [AC_MSG_WARN([Xpm library not found, disabling XPM support])])
fi
AM_CONDITIONAL([HAVE_XPM], [test "x$HAVE_XPM" = "xyes"])

# Check for XFT if enable_xft != no
HAVE_XFT="no"
if test "x$enable_xft" != "xno"; then
  PKG_CHECK_MODULES([XFT], [xft], [HAVE_XFT="yes";
    CFLAGS="$CFLAGS $XFT_CFLAGS";
    LIBS="$LIBS $XFT_LIBS"],
    [AC_MSG_WARN([XFT not found, disabling XFT support])])
fi
AM_CONDITIONAL([HAVE_XFT], [test "x$HAVE_XFT" = "xyes"])

# Check for XCursor if enable_xcursor != no
HAVE_XCURSOR="no"
if test "x$enable_xcursor" != "xno"; then
  PKG_CHECK_MODULES([XCURSOR], [xcursor],
    [HAVE_XCURSOR="yes";
     CFLAGS="$CFLAGS $XCURSOR_CFLAGS";
     LIBS="$LIBS $XCURSOR_LIBS"],
    [AC_MSG_WARN([Xcursor not found, disabling XCursor support])])
fi
AM_CONDITIONAL([HAVE_XCURSOR], [test "x$HAVE_XCURSOR" = "xyes"])

# Check for Xinerama if enable_xinerama != no
HAVE_XINERAMA="no"
if test "x$enable_xinerama" != "xno"; then
  PKG_CHECK_MODULES([XINERAMA], [xinerama],
    [HAVE_XINERAMA="yes";
     CFLAGS="$CFLAGS $XINERAMA_CFLAGS";
     LIBS="$LIBS $XINERAMA_LIBS"],
    [AC_MSG_WARN([Xinerama not found, disabling Xinerama support])])
fi
AM_CONDITIONAL([HAVE_XINERAMA], [test "x$HAVE_XINERAMA" = "xyes"])


# -----------------------------
# Output summary
# -----------------------------
AC_MSG_NOTICE([
----------------------------------------
  Gadgets:     $enable_gadgets
  XPM:         $HAVE_XPM
  XFT:         $HAVE_XFT
  XCursor:     $HAVE_XCURSOR
  Xinerama:    $HAVE_XINERAMA
----------------------------------------
])

# -----------------------------
# Create config.h with #defines
# -----------------------------
# We define C preprocessor symbols so you can do #ifdef HAVE_XPM, etc.

if test "x$HAVE_XPM" = "xyes"; then
  AC_DEFINE([HAVE_XPM], [1], [Define if XPM is enabled])
fi

if test "x$HAVE_XFT" = "xyes"; then
  AC_DEFINE([HAVE_XFT], [1], [Define if XFT is enabled])
fi

if test "x$HAVE_XCURSOR" = "xyes"; then
  AC_DEFINE([HAVE_XCURSOR], [1], [Define if XCursor is enabled])
fi

if test "x$HAVE_XINERAMA" = "xyes"; then
  AC_DEFINE([HAVE_XINERAMA], [1], [Define if Xinerama is enabled])
fi

if test "x$enable_gadgets" = "xyes"; then
  AC_DEFINE([ENABLE_GADGETS], [1], [Define if building dzen2 gadgets])
fi


# -----------------------------
# Generate Makefiles
# -----------------------------
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  gadgets/Makefile
])
AC_OUTPUT
