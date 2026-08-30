#!/usr/bin/env bash
set -u

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SOURCE_ROOT=$(cd "${DZEN2_TEST_SOURCE_ROOT:-$SCRIPT_DIR/../../..}" && pwd)
BUILD_ROOT=$(cd "${DZEN2_TEST_BUILD_ROOT:-$SOURCE_ROOT}" && pwd)
DZEN2_BINARY=${DZEN2_TEST_BINARY:-$BUILD_ROOT/src/dzen2}
# shellcheck source=../test_common.sh
. "$SOURCE_ROOT/tests/integration/test_common.sh"

EXPECTED_DIR=$SCRIPT_DIR/expected
ACTUAL_DIR=$BUILD_ROOT/tests/integration/xinerama/actual
DIFFS_DIR=$BUILD_ROOT/tests/integration/xinerama/diffs
XEPHYR_PID=
XVFB_PID=
DZEN_PID=
COMPARE_CMD=()
CAPTURE_CMD=()

test_announce \
  'INFO: Xinerama integration test plan:' \
  '      - start a parent Xvfb and a nested three-screen Xephyr server' \
  '      - render dzen on Xinerama screen index 2' \
  '      - capture the nested root window and compare it with expected/test.png' \
  "      - write captures, diffs, and X server logs below $(test_project_path "$ACTUAL_DIR") and $(test_project_path "$DIFFS_DIR")"
if [ "${UPDATE_XINERAMA_EXPECTED:-0}" = 1 ]; then
  test_announce '      - UPDATE_XINERAMA_EXPECTED=1: replace the expected screenshot'
fi

cleanup() {
  if [ -n "$DZEN_PID" ]; then kill "$DZEN_PID" 2>/dev/null || true; wait "$DZEN_PID" 2>/dev/null || true; fi
  if [ -n "$XEPHYR_PID" ]; then kill "$XEPHYR_PID" 2>/dev/null || true; wait "$XEPHYR_PID" 2>/dev/null || true; fi
  if [ -n "$XVFB_PID" ]; then kill "$XVFB_PID" 2>/dev/null || true; wait "$XVFB_PID" 2>/dev/null || true; fi
}
trap cleanup EXIT INT TERM

if command -v magick >/dev/null 2>&1 && magick -version 2>/dev/null | head -1 | grep -q 'ImageMagick 7'; then
  COMPARE_CMD=(magick compare)
  CAPTURE_CMD=(magick import)
elif command -v compare >/dev/null 2>&1 && command -v import >/dev/null 2>&1; then
  COMPARE_CMD=(compare)
  CAPTURE_CMD=(import)
else
  printf '%sFAIL:%s ImageMagick 6 (compare/import) or ImageMagick 7 (magick) is required\n' "$RED" "$RESET" >&2
  exit 1
fi

test_require_commands Xvfb Xephyr timeout xset seq stat || exit 1
[ -x "$DZEN2_BINARY" ] || { printf '%sFAIL:%s dzen2 test binary is not built: %s\n' "$RED" "$RESET" "$DZEN2_BINARY" >&2; exit 1; }
mkdir -p "$EXPECTED_DIR" "$ACTUAL_DIR" "$DIFFS_DIR"

PARENT_DISPLAY=$(test_find_free_display) || { printf '%sFAIL:%s no free X display\n' "$RED" "$RESET" >&2; exit 1; }
printf '%sTEST:%s starting parent Xvfb on %s\n' "$GRAY" "$RESET" "$PARENT_DISPLAY"
if [ "$(stat -c %u /tmp/.X11-unix 2>/dev/null || printf 1)" = 0 ]; then
  XVFB_TRANSPORT=(-nolisten tcp)
  PARENT_CLIENT_DISPLAY=$PARENT_DISPLAY
else
  XVFB_TRANSPORT=(-nolisten unix -nolisten local -listen tcp)
  PARENT_CLIENT_DISPLAY=127.0.0.1:${PARENT_DISPLAY#:}
fi
Xvfb "$PARENT_DISPLAY" -screen 0 640x480x24 "${XVFB_TRANSPORT[@]}" >"$ACTUAL_DIR/Xvfb.log" 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 100); do
  DISPLAY="$PARENT_CLIENT_DISPLAY" xset q >/dev/null 2>&1 && break
  kill -0 "$XVFB_PID" 2>/dev/null || break
  sleep .05
done
if ! DISPLAY="$PARENT_CLIENT_DISPLAY" xset q >/dev/null 2>&1; then
  printf '%sFAIL:%s parent Xvfb failed to start; see %s/Xvfb.log\n' "$RED" "$RESET" "$ACTUAL_DIR" >&2
  exit 1
fi

DISPLAY_NUM=$(test_find_free_display) || { printf '%sFAIL:%s no free X display\n' "$RED" "$RESET" >&2; exit 1; }
if [ "$(stat -c %u /tmp/.X11-unix 2>/dev/null || printf 1)" = 0 ]; then
  XEPHYR_TRANSPORT=(-nolisten tcp)
  CLIENT_DISPLAY=$DISPLAY_NUM
else
  XEPHYR_TRANSPORT=(-nolisten unix -nolisten local -listen tcp)
  CLIENT_DISPLAY=127.0.0.1:${DISPLAY_NUM#:}
fi
printf '%sTEST:%s starting Xephyr on %s with three Xinerama screens\n' "$GRAY" "$RESET" "$DISPLAY_NUM"
DISPLAY="$PARENT_CLIENT_DISPLAY" Xephyr +xinerama -ac "${XEPHYR_TRANSPORT[@]}" \
  -screen 100x100+0+0 \
  -screen 100x100+100+0 \
  -screen 100x100+200+0 \
  "$DISPLAY_NUM" >"$ACTUAL_DIR/Xephyr.log" 2>&1 &
XEPHYR_PID=$!

for _ in $(seq 1 100); do
  DISPLAY="$CLIENT_DISPLAY" xset q >/dev/null 2>&1 && break
  kill -0 "$XEPHYR_PID" 2>/dev/null || break
  sleep .05
done
if ! DISPLAY="$CLIENT_DISPLAY" xset q >/dev/null 2>&1; then
  printf '%sFAIL:%s Xephyr failed to start; see %s/Xephyr.log\n' "$RED" "$RESET" "$ACTUAL_DIR" >&2
  exit 1
fi

printf '%sTEST:%s rendering dzen on Xinerama screen index 2 (the third screen)\n' "$GRAY" "$RESET"
DISPLAY="$CLIENT_DISPLAY" timeout 3s "$DZEN2_BINARY" \
  -xs 2 -expand right -h 30 -bg '#ff0000' -p &
DZEN_PID=$!
sleep 1

ACTUAL_FILE=$ACTUAL_DIR/test.png
EXPECTED_FILE=$EXPECTED_DIR/test.png
DIFF_FILE=$DIFFS_DIR/test.png
if ! DISPLAY="$CLIENT_DISPLAY" "${CAPTURE_CMD[@]}" -window root "$ACTUAL_FILE"; then
  printf '%sFAIL:%s failed to capture the Xephyr root window\n' "$RED" "$RESET" >&2
  exit 1
fi

kill "$DZEN_PID" 2>/dev/null || true
wait "$DZEN_PID" 2>/dev/null || true
DZEN_PID=

if [ "${UPDATE_XINERAMA_EXPECTED:-0}" = 1 ]; then
  cp "$ACTUAL_FILE" "$EXPECTED_FILE"
  rm -f "$DIFF_FILE"
  printf '%sPASS:%s updated %s\n' "$GREEN" "$RESET" "$EXPECTED_FILE"
  exit 0
fi

if [ ! -f "$EXPECTED_FILE" ]; then
  printf '%sFAIL:%s missing expected screenshot %s\n' "$RED" "$RESET" "$EXPECTED_FILE" >&2
  printf '%sINFO:%s review %s, then run with UPDATE_XINERAMA_EXPECTED=1\n' "$GRAY" "$RESET" "$ACTUAL_FILE" >&2
  exit 1
fi

raw_diff=$(DISPLAY="$CLIENT_DISPLAY" "${COMPARE_CMD[@]}" -metric AE -fuzz 5% \
  "$EXPECTED_FILE" "$ACTUAL_FILE" "$DIFF_FILE" 2>&1 || true)
diff_pixels=$(printf '%s\n' "$raw_diff" | sed 's/[[:space:]]*(.*//' | grep -o '^[0-9]*' | head -1)
if [ "$diff_pixels" = 0 ]; then
  rm -f "$DIFF_FILE"
  printf '%sPASS:%s Xinerama screenshot matches\n' "$GREEN" "$RESET"
else
  printf '%sFAIL:%s Xinerama screenshot differs by %s pixels; see %s\n' \
    "$RED" "$RESET" "${diff_pixels:-unknown}" "$DIFF_FILE" >&2
  exit 1
fi
