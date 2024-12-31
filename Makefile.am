# -- Makefile.am --

SUBDIRS = src

if ENABLE_GADGETS
  SUBDIRS += gadgets
endif

# "make dist" can be configured to include all your needed files:
EXTRA_DIST = \
  CREDITS \
  LICENSE \
  INSTALL \
  README \
  README.dzen \
  help \
  run_integration_tests \
  TESTS.md \
  .gitignore \
  shell.nix \
  .envrc

.PHONY: test

test:
	./run_integration_tests
