# We'll build multiple small executables: dbar, gdbar, gcpubar, textwidth
bin_PROGRAMS = dbar gdbar gcpubar textwidth

# Common sources used by multiple programs
common_sources = dbar.c dbar.h

# dbar program
dbar_SOURCES = dbar-main.c dbar.c
dbar_LDADD = $(LIBS)

# gdbar program
gdbar_SOURCES = gdbar.c dbar.c
gdbar_LDADD = $(LIBS)

# gcpubar
gcpubar_SOURCES = gcpubar.c dbar.c
gcpubar_LDADD = $(LIBS)

# textwidth
textwidth_SOURCES = textwidth.c
textwidth_LDADD = $(LIBS)
