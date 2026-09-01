# -- Makefile.am --

SUBDIRS = src

if ENABLE_GADGETS
  SUBDIRS += gadgets
endif

bin_SCRIPTS = dzen2-help
CLEANFILES = $(bin_SCRIPTS) .x11-test.lock
dist_doc_DATA = README.dzen README.md

dist_man_MANS = dzen2.1

EXTRA_DIST = \
  CREDITS \
  LICENSE \
  INSTALL \
  docs/feature-borders.md \
  bitmaps \
  dzen2-help.in \
  tests/integration/test_common.sh \
  tests/integration/test_compare_metric.sh \
  tests/integration/signals/test.sh \
  tests/integration/xinerama/test.sh \
  tests/integration/xinerama/.gitignore \
  tests/integration/xinerama/expected \
  tests/integration/xrandr/test.sh \
  tests/integration/xrandr/.gitignore \
  tests/integration/xrandr/expected \
  tests/integration/visual/runner.sh \
  tests/integration/visual/xft/cases.md \
  tests/integration/visual/xft/.gitignore \
  tests/integration/visual/xft/expected \
  tests/integration/visual/borders/cases.md \
  tests/integration/visual/borders/.gitignore \
  tests/integration/visual/borders/expected \
  tests/integration/visual/core-font/cases.md \
  tests/integration/visual/core-font/.gitignore \
  tests/integration/visual/core-font/expected \
  tests/tools/performance_benchmark.sh \
  .gitignore \
  flake.nix \
  flake.lock \
  .envrc

noinst_HEADERS = tests/unit/test_common.h

UNIT_TESTS = \
  tests/unit/action_test \
  tests/unit/border_test \
  tests/unit/layout_test \
  tests/unit/signal_dispatch_test \
  tests/unit/text_buffer_test

TEST_HELPERS = \
  tests/helpers/blocked_signal_exec \
  tests/helpers/high_fd_exec

check_PROGRAMS = $(UNIT_TESTS) $(TEST_HELPERS)
TESTS = $(UNIT_TESTS)

tests_unit_action_test_SOURCES = \
  tests/unit/action_test.c \
  src/action.c \
  src/action.h \
  src/text_buffer.c \
  src/text_buffer.h
tests_unit_action_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/tests/unit
tests_unit_action_test_LDADD = $(X11_LIBS)

tests_unit_border_test_SOURCES = \
  tests/unit/border_test.c \
  src/border.c \
  src/border.h
tests_unit_border_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/tests/unit
tests_unit_border_test_LDADD = $(X11_LIBS)

tests_unit_layout_test_SOURCES = \
  tests/unit/layout_test.c \
  src/border.h \
  src/layout.c \
  src/layout.h
tests_unit_layout_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/tests/unit
tests_unit_layout_test_LDADD = $(X11_LIBS)

tests_unit_signal_dispatch_test_SOURCES = \
  tests/unit/signal_dispatch_test.c \
  src/signal_dispatch.c \
  src/signal_dispatch.h
tests_unit_signal_dispatch_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/tests/unit

tests_unit_text_buffer_test_SOURCES = \
  tests/unit/text_buffer_test.c \
  src/text_buffer.c \
  src/text_buffer.h \
  src/line_reader.c \
  src/line_reader.h \
  src/util.c \
  src/util.h
tests_unit_text_buffer_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/tests/unit

tests_helpers_blocked_signal_exec_SOURCES = tests/helpers/blocked_signal_exec.c
tests_helpers_high_fd_exec_SOURCES = tests/helpers/high_fd_exec.c

TEST_ENV = \
  DZEN2_TEST_SOURCE_ROOT="$(abs_top_srcdir)" \
  DZEN2_TEST_BUILD_ROOT="$(abs_top_builddir)" \
  DZEN2_TEST_BINARY="$(abs_top_builddir)/src/dzen2"

X11_TEST_LOCK = $(abs_top_builddir)/.x11-test.lock
X11_TEST_RUN = flock "$(X11_TEST_LOCK)"

do_subst = sed -e 's,[@]bindir[@],$(bindir),g' \
            -e 's,[@]docdir[@],$(docdir),g'

dzen2-help: dzen2-help.in Makefile
	$(do_subst) < $(srcdir)/dzen2-help.in > dzen2-help
	chmod +x dzen2-help

.PHONY: test test-unit test-action test-border test-layout test-signal-dispatch test-text-buffer test-compare-metric \
  test-signals test-visual test-visual-xft test-visual-borders test-visual-core test-nonxft \
  test-xinerama test-xrandr update-man

test: check

test-unit: $(UNIT_TESTS)
	@set -e; for test_program in $(UNIT_TESTS); do ./$$test_program; done

test-action: tests/unit/action_test
	./tests/unit/action_test

test-border: tests/unit/border_test
	./tests/unit/border_test

test-layout: tests/unit/layout_test
	./tests/unit/layout_test

test-signal-dispatch: tests/unit/signal_dispatch_test
	./tests/unit/signal_dispatch_test

test-text-buffer: tests/unit/text_buffer_test
	./tests/unit/text_buffer_test

test-signals: all $(TEST_HELPERS)
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/signals/test.sh

test-compare-metric:
	@$(TEST_ENV) $(top_srcdir)/tests/integration/test_compare_metric.sh

if HAVE_XFT
test-visual: test-visual-xft

test-visual-xft: all
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/visual/runner.sh \
		$(abs_top_srcdir)/tests/integration/visual/xft/cases.md

test-visual-borders: all
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/visual/runner.sh \
		$(abs_top_srcdir)/tests/integration/visual/borders/cases.md

test-visual-core:
	@echo "test-visual-core requires a build configured with --disable-xft" >&2
	@exit 1
else
test-visual: test-visual-core

test-visual-xft:
	@echo "test-visual-xft requires a build configured with --enable-xft" >&2
	@exit 1

test-visual-borders:
	@echo "test-visual-borders requires a build configured with --enable-xft" >&2
	@exit 1

test-visual-core: all
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/visual/runner.sh \
		$(abs_top_srcdir)/tests/integration/visual/core-font/cases.md
endif

test-nonxft: test-visual-core

if HAVE_XINERAMA
test-xinerama: all
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/xinerama/test.sh
else
test-xinerama:
	@echo "test-xinerama requires a build configured with --enable-xinerama" >&2
	@exit 1
endif

if HAVE_XRANDR
if HAVE_XFT
test-xrandr: all
	@$(TEST_ENV) $(X11_TEST_RUN) $(top_srcdir)/tests/integration/xrandr/test.sh
else
test-xrandr:
	@echo "test-xrandr requires a build configured with --enable-xrandr and --enable-xft" >&2
	@exit 1
endif
else
test-xrandr:
	@echo "test-xrandr requires a build configured with --enable-xrandr and --enable-xft" >&2
	@exit 1
endif

CHECK_LOCAL_TARGETS = test-compare-metric test-signals test-visual

if HAVE_XFT
CHECK_LOCAL_TARGETS += test-visual-borders
endif

if HAVE_XINERAMA
CHECK_LOCAL_TARGETS += test-xinerama
endif

if HAVE_XRANDR
if HAVE_XFT
CHECK_LOCAL_TARGETS += test-xrandr
endif
endif

check-local: $(CHECK_LOCAL_TARGETS)

# Integration tests keep diagnostic artifacts in the build tree.  Remove only
# those generated directories so out-of-tree builds pass distcleancheck.
clean-local:
	$(RM) -r \
	  tests/integration/visual/xft/actual \
	  tests/integration/visual/xft/diffs \
	  tests/integration/visual/core-font/actual \
	  tests/integration/visual/core-font/diffs \
	  tests/integration/visual/borders/actual \
	  tests/integration/visual/borders/diffs \
	  tests/integration/xinerama/actual \
	  tests/integration/xinerama/diffs \
	  tests/integration/xrandr/actual \
	  tests/integration/xrandr/diffs

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
