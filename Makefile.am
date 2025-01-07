# -- Makefile.am --

SUBDIRS = src

if ENABLE_GADGETS
  SUBDIRS += gadgets
endif

bin_SCRIPTS = dzen2-help
CLEANFILES = $(bin_SCRIPTS)
dist_doc_DATA = README.dzen README.md

dist_man_MANS = dzen2.1

EXTRA_DIST = \
  CREDITS \
  LICENSE \
  INSTALL \
  dzen2-help.in \
  integration-tests \
  run_integration_tests \
  TESTS.md \
  .gitignore \
  shell.nix \
  .envrc

do_subst = sed -e 's,[@]bindir[@],$(bindir),g' \
            -e 's,[@]docdir[@],$(docdir),g'

dzen2-help: dzen2-help.in Makefile
	$(do_subst) < $(srcdir)/dzen2-help.in > dzen2-help
	chmod +x dzen2-help

.PHONY: test update-man

test:
	./run_integration_tests

update-man: ./README.md
	pandoc ./README.md \
		--metadata date="$(shell date +%Y-%m-%d)" \
		--metadata author="Robert Manea" \
		--metadata author="Olexandr Sydorchuk" \
		--metadata version="$(VERSION)" \
		--metadata title="$(PACKAGE)" \
		--metadata section="1" \
		-s -t man -o dzen2.1
	$(MAKE) -C gadgets update-man || exit 1;

./README.md: ./README.dzen
# 1) Temporarily replace ^^tag(...) with <DC>tag(...)
# 2) Remove single-caret ^tag(...)
# 3) Convert <DC>tag(...) back into ^tag(...)
	@echo "Updating README.md from README.dzen..."
	sed -E \
	  -e 's/\^\^([^()]+)\(([^)]*)\)/<DC>\1(\2)/g' \
	  -e 's/\^([^()]+)\([^)]*\)//g' \
	  -e 's/<DC>/^/g' \
	  README.dzen > README.md
	@echo "README has been updated."
