# We'll build multiple small executables: dbar, gdbar, gcpubar, textwidth
bin_PROGRAMS = dbar gdbar gcpubar textwidth
CLEANFILES = dbar.1.raw dbar.1.tmp gcpubar.1.raw gcpubar.1.tmp gdbar.1.raw gdbar.1.tmp
MAN_GENERATOR = $(top_srcdir)/build-aux/generate-man.sh

# Common sources used by multiple programs
common_sources = dbar.c dbar.h

# dbar program
dbar_SOURCES = dbar-main.c $(common_sources)
dbar_LDADD = $(LIBS)

# gdbar program
gdbar_SOURCES = gdbar.c $(common_sources)
gdbar_LDADD = $(LIBS)

# gcpubar
gcpubar_SOURCES = gcpubar.c $(common_sources)
gcpubar_LDADD = $(LIBS)

# textwidth
textwidth_SOURCES = textwidth.c
textwidth_LDADD = $(LIBS)

dist_doc_DATA = README.dbar.md \
	README.gcpubar.md \
	README.gdbar.md \
	README.textwidth.md

dist_man_MANS = dbar.1 \
	gcpubar.1 \
	gdbar.1

.PHONY: update-man

update-man: dbar.1 gcpubar.1 gdbar.1

dbar.1: README.dbar.md $(top_srcdir)/configure.ac $(MAN_GENERATOR)
	$(SHELL) $(MAN_GENERATOR) "$<" "$@" "$(PACKAGE)" "$(VERSION)" 1 "Robert Manea"

gcpubar.1: README.gcpubar.md $(top_srcdir)/configure.ac $(MAN_GENERATOR)
	$(SHELL) $(MAN_GENERATOR) "$<" "$@" "$(PACKAGE)" "$(VERSION)" 1 "Robert Manea"

gdbar.1: README.gdbar.md $(top_srcdir)/configure.ac $(MAN_GENERATOR)
	$(SHELL) $(MAN_GENERATOR) "$<" "$@" "$(PACKAGE)" "$(VERSION)" 1 "Robert Manea"
