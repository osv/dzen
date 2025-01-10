# We'll build multiple small executables: dbar, gdbar, gcpubar, textwidth
bin_PROGRAMS = dbar gdbar gcpubar textwidth

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

update-man:
	pandoc ./README.dbar.md \
		--metadata date="$(shell date +%Y-%m-%d)" \
		--metadata author="Robert Manea" \
		--metadata version="$(VERSION)" \
		--metadata title="$(PACKAGE)" \
		--metadata section="1" \
		-s -t man -o dbar.1

	pandoc ./README.gcpubar.md \
		--metadata date="$(shell date +%Y-%m-%d)" \
		--metadata author="Robert Manea" \
		--metadata version="$(VERSION)" \
		--metadata title="$(PACKAGE)" \
		--metadata section="1" \
		-s -t man -o gcpubar.1

	pandoc ./README.gdbar.md \
		--metadata date="$(shell date +%Y-%m-%d)" \
		--metadata author="Robert Manea" \
		--metadata version="$(VERSION)" \
		--metadata title="$(PACKAGE)" \
		--metadata section="1" \
		-s -t man -o gdbar.1
