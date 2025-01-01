# -- Makefile.am --

SUBDIRS = src

if ENABLE_GADGETS
  SUBDIRS += gadgets
endif

EXTRA_DIST = \
  CREDITS \
  LICENSE \
  INSTALL \
  README \
  README.dzen \
  help \
  integration-tests \
  run_integration_tests \
  TESTS.md \
  .gitignore \
  shell.nix \
  .envrc

.PHONY: test

test:
	./run_integration_tests
