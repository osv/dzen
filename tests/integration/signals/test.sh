#!/usr/bin/env bash
set -u

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SOURCE_ROOT=$(cd "${DZEN2_TEST_SOURCE_ROOT:-$SCRIPT_DIR/../../..}" && pwd)
BUILD_ROOT=$(cd "${DZEN2_TEST_BUILD_ROOT:-$SOURCE_ROOT}" && pwd)
DZEN2_BINARY=${DZEN2_TEST_BINARY:-$BUILD_ROOT/src/dzen2}
# shellcheck source=../test_common.sh
. "$SOURCE_ROOT/tests/integration/test_common.sh"
XVFB_PID=
DZEN_PID=
TEST_TMP=
FAILURES=0
PASSES=0
SKIPS=0

test_announce \
  'INFO: Signal integration test plan:' \
  '      - start dzen on an isolated Xvfb display' \
  '      - send termination and user signals and verify actions/exit statuses' \
  '      - exercise inherited signals, high file descriptors, and X disconnects' \
  '      - keep temporary diagnostics only until the suite exits'

cleanup() {
  if [ -n "$DZEN_PID" ]; then kill "$DZEN_PID" 2>/dev/null || true; wait "$DZEN_PID" 2>/dev/null || true; fi
  if [ -n "$XVFB_PID" ]; then kill "$XVFB_PID" 2>/dev/null || true; wait "$XVFB_PID" 2>/dev/null || true; fi
  if [ -n "$TEST_TMP" ] && [ -d "$TEST_TMP" ]; then rm -rf -- "$TEST_TMP"; fi
}
trap cleanup EXIT INT TERM

pass() { PASSES=$((PASSES + 1)); printf '%sPASS:%s %s\n' "$GREEN" "$RESET" "$*"; }
fail() { FAILURES=$((FAILURES + 1)); printf '%sFAIL:%s %s\n' "$RED" "$RESET" "$*" >&2; }
skip() { SKIPS=$((SKIPS + 1)); printf '%sSKIP:%s %s\n' "$YELLOW" "$RESET" "$*"; }
test_case() { printf '%sTEST %s:%s %s\n' "$GRAY" "$1" "$RESET" "$2"; }
summary() {
  echo '---------------------'
  printf '%sPASS:%s %d  %sFAIL:%s %d  %sSKIP:%s %d\n' \
    "$GREEN" "$RESET" "$PASSES" "$RED" "$RESET" "$FAILURES" "$YELLOW" "$RESET" "$SKIPS"
}

test_require_commands Xvfb xset mktemp seq stat || exit 1
[ -x "$DZEN2_BINARY" ] || { echo "dzen2 test binary is not built: $DZEN2_BINARY" >&2; exit 1; }
[ -x "$BUILD_ROOT/tests/helpers/blocked_signal_exec" ] || { echo "blocked_signal_exec is not built" >&2; exit 1; }
[ -x "$BUILD_ROOT/tests/helpers/high_fd_exec" ] || { echo "high_fd_exec is not built" >&2; exit 1; }

TEST_TMP=$(mktemp -d "${TMPDIR:-/tmp}/dzen-signals.XXXXXX")
DISPLAY_NUMBER=$(test_find_free_display) || { echo "no free X display" >&2; exit 1; }
if [ "$(stat -c %u /tmp/.X11-unix 2>/dev/null || printf 1)" = 0 ]; then
  XVFB_TRANSPORT=(-nolisten tcp)
  DISPLAY=$DISPLAY_NUMBER
else
  XVFB_TRANSPORT=(-nolisten unix -nolisten local -listen tcp)
  DISPLAY=127.0.0.1:${DISPLAY_NUMBER#:}
fi
export DISPLAY
Xvfb "$DISPLAY_NUMBER" -screen 0 640x480x24 "${XVFB_TRANSPORT[@]}" >"$TEST_TMP/xvfb.log" 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 100); do
  xset q >/dev/null 2>&1 && break
  sleep .05
done
xset q >/dev/null 2>&1 || { echo "Xvfb failed to start" >&2; exit 1; }

start_dzen() {
  local output=$1
  shift
  "$DZEN2_BINARY" -p "$@" < /dev/null >"$output" 2>"$TEST_TMP/dzen.err" &
  DZEN_PID=$!
  sleep .15
  kill -0 "$DZEN_PID" 2>/dev/null || { fail "dzen exited before the test signal"; DZEN_PID=; return 1; }
}

wait_status() {
  local expected=$1 status
  set +e
  wait "$DZEN_PID"
  status=$?
  set -e
  DZEN_PID=
  if [ "$status" -eq "$expected" ]; then pass "exit status $expected"; else fail "expected status $expected, got $status"; fi
}

set -e
test_case 01 'SIGTERM without onexit returns 143'
start_dzen "$TEST_TMP/term.out"
kill -TERM "$DZEN_PID"
wait_status 143

test_case 02 'SIGTERM runs onexit exactly once'
start_dzen "$TEST_TMP/onexit.out" -e 'onexit=print:MARKER;button3=exit:9'
kill -TERM "$DZEN_PID"
wait_status 143
[ "$(grep -c '^MARKER$' "$TEST_TMP/onexit.out" || true)" -eq 1 ] && pass "SIGTERM onexit runs once" || fail "SIGTERM onexit did not run once"

test_case 03 'onexit exit:N does not override SIGTERM status'
start_dzen "$TEST_TMP/term-exit.out" -e 'onexit=exit:7'
kill -TERM "$DZEN_PID"
wait_status 143

test_case 04 'onexit exit:N does not override timeout status'
"$DZEN2_BINARY" -p 1 -e 'onexit=print:MARKER,exit:7' < /dev/null >"$TEST_TMP/timeout.out" 2>"$TEST_TMP/dzen.err" &
DZEN_PID=$!
wait_status 0
[ "$(grep -c '^MARKER$' "$TEST_TMP/timeout.out" || true)" -eq 1 ] && pass "timeout onexit runs once" || fail "timeout onexit did not run once"

test_case 05 'SIGUSR actions run without terminating dzen'
start_dzen "$TEST_TMP/usr.out" -e 'sigusr1=print:USR1;sigusr2=print:USR2'
kill -USR1 "$DZEN_PID"
kill -USR2 "$DZEN_PID"
sleep .15
kill -0 "$DZEN_PID" 2>/dev/null && pass "SIGUSR actions keep dzen running" || fail "SIGUSR action terminated dzen"
kill -TERM "$DZEN_PID"
wait_status 143
[ "$(grep -c '^USR1$' "$TEST_TMP/usr.out" || true)" -eq 1 ] && pass "SIGUSR1 action" || fail "SIGUSR1 action missing"
[ "$(grep -c '^USR2$' "$TEST_TMP/usr.out" || true)" -eq 1 ] && pass "SIGUSR2 action" || fail "SIGUSR2 action missing"

test_case 06 'SIGUSR exit:N preserves the requested status'
start_dzen "$TEST_TMP/usr-exit.out" -e 'sigusr1=exit:5'
kill -USR1 "$DZEN_PID"
wait_status 5

test_case 07 'ordinary exit:N preserves the requested status'
"$DZEN2_BINARY" -p -e 'onstart=exit:9' < /dev/null >"$TEST_TMP/ordinary-exit.out" 2>"$TEST_TMP/dzen.err" &
DZEN_PID=$!
wait_status 9

test_case 08 'onexit exec does not inherit ignored SIGTERM'
ONEXIT_PID_FILE="$TEST_TMP/onexit-pid"
ONEXIT_MARKER="$TEST_TMP/onexit-after-signal"
ONEXIT_SCRIPT="$TEST_TMP/onexit-signal.sh"
{
  echo '#!/bin/sh'
  printf "printf '%%s\\n' \"\$\$\" > '%s'\n" "$ONEXIT_PID_FILE"
  echo 'kill -TERM $$'
  printf "touch '%s'\n" "$ONEXIT_MARKER"
} >"$ONEXIT_SCRIPT"
chmod +x "$ONEXIT_SCRIPT"
start_dzen "$TEST_TMP/onexit-signal.out" -e "onexit=exec:exec $ONEXIT_SCRIPT"
kill -TERM "$DZEN_PID"
wait_status 143
for _ in $(seq 1 100); do
  [ -s "$ONEXIT_PID_FILE" ] && break
  sleep .02
done
if [ ! -s "$ONEXIT_PID_FILE" ]; then
  fail "onexit exec command did not start"
else
  ONEXIT_PID=$(cat "$ONEXIT_PID_FILE")
  for _ in $(seq 1 100); do
    kill -0 "$ONEXIT_PID" 2>/dev/null || break
    sleep .02
  done
  if kill -0 "$ONEXIT_PID" 2>/dev/null; then
    fail "onexit exec command did not terminate"
  elif [ -e "$ONEXIT_MARKER" ]; then
    fail "onexit exec command inherited an ignored SIGTERM"
  else
    pass "onexit exec resets SIGTERM to the default disposition"
  fi
fi

test_case 09 'X connection and signal pipe work above FD_SETSIZE'
"$BUILD_ROOT/tests/helpers/high_fd_exec" "$DZEN2_BINARY" -p < /dev/null >"$TEST_TMP/high-fd.out" 2>"$TEST_TMP/high-fd.err" &
DZEN_PID=$!
sleep .15
if ! kill -0 "$DZEN_PID" 2>/dev/null; then
  set +e
  wait "$DZEN_PID"
  HIGH_FD_STATUS=$?
  set -e
  DZEN_PID=
  if [ "$HIGH_FD_STATUS" -eq 77 ]; then
    skip "the environment could not reserve descriptors through FD_SETSIZE"
  else
    fail "high-fd dzen exited early with status $HIGH_FD_STATUS"
  fi
else
  kill -TERM "$DZEN_PID"
  wait_status 143
fi

test_case 10 'inherited blocked SIGTERM is enabled by the dispatcher'
"$BUILD_ROOT/tests/helpers/blocked_signal_exec" "$DZEN2_BINARY" -p < /dev/null >"$TEST_TMP/blocked-term.out" 2>"$TEST_TMP/blocked-term.err" &
DZEN_PID=$!
sleep .15
if kill -0 "$DZEN_PID" 2>/dev/null; then
  kill -TERM "$DZEN_PID"
  wait_status 143
else
  fail "dzen with inherited blocked SIGTERM exited before the test signal"
  DZEN_PID=
fi

test_case 11 'X connection loss does not leave dzen in a poll busy-loop'
start_dzen "$TEST_TMP/x-disconnect.out"
kill "$XVFB_PID"
wait "$XVFB_PID" 2>/dev/null || true
XVFB_PID=
for _ in $(seq 1 100); do
  kill -0 "$DZEN_PID" 2>/dev/null || break
  sleep .02
done
if kill -0 "$DZEN_PID" 2>/dev/null; then
  fail "dzen remained running after the X connection closed"
  kill -TERM "$DZEN_PID"
  wait "$DZEN_PID" 2>/dev/null || true
  DZEN_PID=
else
  wait_status 1
fi

summary
if [ "$FAILURES" -ne 0 ]; then exit 1; fi
echo "signal integration tests passed"
