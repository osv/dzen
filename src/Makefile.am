# -- src/Makefile.am --

# The final binary is named dzen2
bin_PROGRAMS = dzen2

# Source files for building dzen2
dzen2_SOURCES = \
  action.c \
  action.h \
  caches.c \
  draw.c \
  dzen.h \
  font.c \
  font.h \
  kvstore.c \
  kvstore.h \
  layout.c \
  layout.h \
  line_reader.c \
  line_reader.h \
  main.c \
  text_buffer.c \
  text_buffer.h \
  util.c \
  util.h \
  xrandr.c \
  xrandr.h

# We can add extra CFLAGS or LIBS if needed, e.g.

dzen2_CPPFLAGS = $(CPPFLAGS)
dzen2_LDADD    = $(LIBS)
