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
  test_e2e \
  test_perfomance \
  TESTS.md \
  .gitignore \
  flake.nix \
  flake.lock \
  .envrc

do_subst = sed -e 's,[@]bindir[@],$(bindir),g' \
            -e 's,[@]docdir[@],$(docdir),g'

dzen2-help: dzen2-help.in Makefile
	$(do_subst) < $(srcdir)/dzen2-help.in > dzen2-help
	chmod +x dzen2-help

.PHONY: test update-man

test:
	./test_e2e TESTS.md

# TODO: use scdoc https://github.com/rvaiya/warpd/blob/master/Makefile#L36C2-L36C7
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

format:
	find $(srcdir) -name "*.c" -o -name "*.h" | xargs clang-format -i

check-format:
	find $(srcdir) -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror
