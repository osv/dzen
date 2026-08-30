# -- Makefile.am --

SUBDIRS = src

if ENABLE_GADGETS
  SUBDIRS += gadgets
endif

bin_SCRIPTS = dzen2-help
CLEANFILES = $(bin_SCRIPTS) tests/high_fd_exec
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
  test_signals \
  test_xinerama \
  test_xrandr \
  tests/high_fd_exec.c \
  tests/test_common.h \
  tests/test_layout.c \
  tests/test_signal_dispatch.c \
  tests/test_text_buffer.c \
  TESTS.md \
  TESTS-NONXFT.md \
  .gitignore \
  flake.nix \
  flake.lock \
  .envrc

do_subst = sed -e 's,[@]bindir[@],$(bindir),g' \
            -e 's,[@]docdir[@],$(docdir),g'

dzen2-help: dzen2-help.in Makefile
	$(do_subst) < $(srcdir)/dzen2-help.in > dzen2-help
	chmod +x dzen2-help

.PHONY: test test-layout test-signal-dispatch test-signals test-text-buffer test-xinerama test-xrandr update-man

test:
	$(MAKE) test-layout
	$(MAKE) test-signal-dispatch
	$(MAKE) test-text-buffer
	$(MAKE) test-signals
	./test_e2e TESTS.md

test-layout:
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o tests/test_layout tests/test_layout.c src/layout.c $(X11_LIBS)
	./tests/test_layout
	$(RM) tests/test_layout

test-text-buffer:
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o tests/test_text_buffer tests/test_text_buffer.c \
		src/text_buffer.c src/line_reader.c src/util.c
	./tests/test_text_buffer
	$(RM) tests/test_text_buffer

test-signal-dispatch:
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o tests/test_signal_dispatch tests/test_signal_dispatch.c \
		src/signal_dispatch.c
	./tests/test_signal_dispatch
	$(RM) tests/test_signal_dispatch

test-signals: tests/high_fd_exec
	./test_signals

tests/high_fd_exec: tests/high_fd_exec.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

test-nonxft:
	./test_e2e TESTS-NONXFT.md

test-xinerama:
	./test_xinerama

test-xrandr:
	./test_xrandr

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
