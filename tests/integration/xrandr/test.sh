#!/usr/bin/env bash
# XRandR integration tests using an isolated Xorg dummy server. Additional
# outputs are provisioned when the DDX exposes them.
set -u

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SOURCE_ROOT=$(cd "${DZEN2_TEST_SOURCE_ROOT:-$SCRIPT_DIR/../../..}" && pwd)
BUILD_ROOT=$(cd "${DZEN2_TEST_BUILD_ROOT:-$SOURCE_ROOT}" && pwd)
DZEN2_BINARY=${DZEN2_TEST_BINARY:-$BUILD_ROOT/src/dzen2}
# shellcheck source=../test_common.sh
. "$SOURCE_ROOT/tests/integration/test_common.sh"
SCREEN_W=1024; SCREEN_H=600
ALT_W=800; ALT_H=600
EXPANDED_W=1200; EXPANDED_H=900
BAR_H=28; BAR_X=37
BAR_Y=23; BAR_W=211
TEST_FONT='DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none'
BAR_BG='#203040'; BAR_FG='#ffffff'
TEST_DIR=$SCRIPT_DIR
ACTUAL=$BUILD_ROOT/tests/integration/xrandr/actual; EXPECTED=$TEST_DIR/expected; DIFFS=$BUILD_ROOT/tests/integration/xrandr/diffs
mkdir -p "$ACTUAL" "$EXPECTED" "$DIFFS"
XORG_PID= DZEN_PID= TMPDIR_XRANDR= WIN=
FAILURES=0; PASSES=0; SKIPS=0

pass() { PASSES=$((PASSES + 1)); echo "${GREEN}PASS:${RESET} $*"; }
fail() { FAILURES=$((FAILURES + 1)); echo "${RED}FAIL:${RESET} $*"; }
skip() { SKIPS=$((SKIPS + 1)); echo "${YELLOW}SKIP:${RESET} $*"; }
info() { echo "${GRAY}INFO:${RESET} $*"; }
test_case() {
  if [ -n "${3:-}" ]; then echo "${GRAY}TEST $1:${RESET} $2 ($3)"; else echo "${GRAY}TEST $1:${RESET} $2"; fi
}
summary() { echo "---------------------"; echo "${GREEN}PASS:${RESET} $PASSES  ${YELLOW}SKIP:${RESET} $SKIPS  ${RED}FAIL:${RESET} $FAILURES"; }
test_announce \
  'INFO: XRandR integration test plan:' \
  '      - start an isolated Xorg server with the dummy video driver' \
  '      - change output modes, positions, connections, and framebuffer geometry' \
  '      - verify dzen geometry, window identity, dock properties, and menus' \
  "      - compare screenshots and write diagnostics below $(test_project_path "$ACTUAL") and $(test_project_path "$DIFFS")"
if [ "${UPDATE_XRANDR_EXPECTED:-0}" = 1 ]; then
  test_announce '      - UPDATE_XRANDR_EXPECTED=1: replace expected screenshots with captures'
fi
info "Environment variables:"
info "  XRANDR_TEST_ALLOW_MISSING_BACKEND=1  skip when the Xorg dummy DDX is unavailable"
info "  UPDATE_XRANDR_EXPECTED=1             replace expected screenshots with actual results"
info "  XRANDR_TEST_DISPLAY=:N               use a specific Xorg display instead of finding one"
info "  XRANDR_XORG_MODULE_PATH=PATHS         override the Xorg module search path"
info "  NO_COLOR=1                            disable colored output"
cleanup_dzen() {
  if [ -n "$DZEN_PID" ]; then kill "$DZEN_PID" 2>/dev/null || true; wait "$DZEN_PID" 2>/dev/null || true; DZEN_PID=; fi
}
cleanup() {
  cleanup_dzen
  if [ -n "$XORG_PID" ]; then kill "$XORG_PID" 2>/dev/null || true; wait "$XORG_PID" 2>/dev/null || true; fi
  if [ "$FAILURES" -gt 0 ] && [ -n "$TMPDIR_XRANDR" ]; then
    [ ! -f "$TMPDIR_XRANDR/Xorg.log" ] || cp "$TMPDIR_XRANDR/Xorg.log" "$ACTUAL/Xorg.log"
    [ ! -f "$TMPDIR_XRANDR/Xorg.stdout" ] || cp "$TMPDIR_XRANDR/Xorg.stdout" "$ACTUAL/Xorg.stdout"
  fi
  if [ -n "$TMPDIR_XRANDR" ] && [ -d "$TMPDIR_XRANDR" ]; then rm -rf -- "$TMPDIR_XRANDR"; fi
}
trap cleanup EXIT INT TERM
assert_eq() { if [ "$2" = "$3" ]; then pass "$1 = $3"; else fail "$1 expected $3, got $2"; fi; }
assert_match() { if printf '%s\n' "$2" | grep -Eq "$3"; then pass "$1"; else fail "$1 (got: $2)"; fi; }

if ! test_require_commands Xorg xrandr xdotool xwininfo xprop import compare awk grep mktemp stat fc-match seq; then
  summary
  exit 1
fi
if [ ! -x "$DZEN2_BINARY" ]; then fail "dzen2 test binary is not built: $DZEN2_BINARY"; summary; exit 1; fi
FONT_MATCH=$(fc-match -f '%{family}|%{style}\n' "$TEST_FONT" | head -1)
if ! printf '%s\n' "$FONT_MATCH" | grep -Eq '^DejaVu Sans Mono\|Book'; then
  fail "required screenshot font did not resolve to DejaVu Sans Mono Book (got: $FONT_MATCH)"
  summary
  exit 1
fi

TMPDIR_XRANDR=$(mktemp -d "${TMPDIR:-/tmp}/dzen-xrandr.XXXXXX")
XORG_CONFIG=$TMPDIR_XRANDR/xorg-dummy.conf; XORG_LOG=$TMPDIR_XRANDR/Xorg.log
cat >"$XORG_CONFIG" <<EOF
Section "ServerFlags"
 Option "AutoAddDevices" "false"
 Option "AllowMouseOpenFail" "true"
 Option "DontVTSwitch" "true"
 Option "BlankTime" "0"
EndSection
Section "Device"
 Identifier "DummyDevice"
 Driver "dummy"
 VideoRam 256000
EndSection
Section "Monitor"
 Identifier "DummyMonitor"
 HorizSync 5.0-100.0
 VertRefresh 5.0-100.0
 Modeline "${SCREEN_W}x${SCREEN_H}" 49.00 ${SCREEN_W} 1064 1168 1312 ${SCREEN_H} 603 613 624
 Modeline "${ALT_W}x${ALT_H}" 38.22 ${ALT_W} 832 912 1024 ${ALT_H} 603 607 622
EndSection
Section "Screen"
 Identifier "DummyScreen"
 Device "DummyDevice"
 Monitor "DummyMonitor"
 DefaultDepth 24
 SubSection "Display"
  Depth 24
  Virtual 2048 1200
  Modes "${SCREEN_W}x${SCREEN_H}" "${ALT_W}x${ALT_H}"
 EndSubSection
EndSection
Section "ServerLayout"
 Identifier "DummyLayout"
 Screen "DummyScreen"
EndSection
EOF

if [ -n "${XRANDR_TEST_DISPLAY:-}" ]; then DISPLAY_NUM=$XRANDR_TEST_DISPLAY; else DISPLAY_NUM=$(test_find_free_display) || { fail "no free X display"; summary; exit 1; }; fi
# Xorg rejects the filesystem UNIX transport when /tmp/.X11-unix is not owned
# by root (some containers deliberately map it to nobody).  Use loopback TCP in
# that environment; on normal hosts retain the safer UNIX-only default.
if [ "$(stat -c %u /tmp/.X11-unix 2>/dev/null || printf 1)" = 0 ]; then
  XORG_TRANSPORT=(-nolisten tcp)
  CLIENT_DISPLAY=$DISPLAY_NUM
else
  XORG_TRANSPORT=(-nolisten unix -nolisten local -listen tcp)
  CLIENT_DISPLAY=127.0.0.1:${DISPLAY_NUM#:}
fi
XORG_MODULE_ARGS=()
if [ -n "${XRANDR_XORG_MODULE_PATH:-}" ]; then XORG_MODULE_ARGS=(-modulepath "$XRANDR_XORG_MODULE_PATH"); fi
Xorg "$DISPLAY_NUM" -noreset "${XORG_TRANSPORT[@]}" "${XORG_MODULE_ARGS[@]}" -config "$XORG_CONFIG" -logfile "$XORG_LOG" -ac >"$TMPDIR_XRANDR/Xorg.stdout" 2>&1 &
XORG_PID=$!; export DISPLAY=$CLIENT_DISPLAY
READY=false
for _ in $(seq 1 100); do
  if xrandr --query >/dev/null 2>&1; then READY=true; break; fi
  if ! kill -0 "$XORG_PID" 2>/dev/null; then break; fi
  sleep .05
done
if [ "$READY" != true ]; then
  if grep -q 'Failed to load module "dummy"\|no screens found' "$XORG_LOG" 2>/dev/null; then
    if [ "${XRANDR_TEST_ALLOW_MISSING_BACKEND:-0}" = 1 ]; then
      skip "Xorg dummy DDX unavailable (install xserver-xorg-video-dummy)"
      summary
      exit 0
    fi
    fail "Xorg dummy DDX unavailable (install xserver-xorg-video-dummy or set XRANDR_TEST_ALLOW_MISSING_BACKEND=1)"
    summary
    exit 1
  fi
  fail "isolated Xorg dummy failed; log: $XORG_LOG"; sed -n '1,160p' "$XORG_LOG"; summary; exit 1
fi

active_outputs() { xrandr --query | awk '$2 == "connected" { for (i=3; i<=NF; i++) if ($i ~ /^[0-9]+x[0-9]+[+-]/) { print $1; break } }'; }
connected_outputs() { xrandr --query | awk '$2 == "connected" { print $1 }'; }
randr_outputs() { xrandr --query | awk '$2 == "connected" || $2 == "disconnected" { print $1 }'; }
output_geometry() { xrandr --query | awk -v o="$1" '$1 == o && $2 == "connected" { for (i=3; i<=NF; i++) if ($i ~ /^[0-9]+x[0-9]+[+-]/) { print $i; exit } }'; }
output_connection() { xrandr --query | awk -v o="$1" '$1 == o { print $2; exit }'; }
wait_output_geometry() {
  local output=$1 expected=$2 actual= _
  for _ in $(seq 1 60); do actual=$(output_geometry "$output"); [ "$actual" = "$expected" ] && return 0; sleep .05; done
  return 1
}
window_state() { xwininfo -id "$WIN" 2>/dev/null | awk -F: '/Map State:/ { sub(/^[[:space:]]+/, "", $2); print $2 }'; }
wait_window_state() {
  local expected=$1 actual= _
  for _ in $(seq 1 60); do actual=$(window_state); [ "$actual" = "$expected" ] && return 0; sleep .05; done
  return 1
}
window_geometry() { xdotool getwindowgeometry --shell "$WIN" 2>/dev/null; }
root_geometry() { xwininfo -root 2>/dev/null | awk '/Width:/ { width=$2 } /Height:/ { height=$2 } END { print width "x" height }'; }
wait_root_geometry() {
  local expected=$1 actual= _
  for _ in $(seq 1 60); do actual=$(root_geometry); [ "$actual" = "$expected" ] && return 0; sleep .05; done
  return 1
}
wait_window_geometry() {
  local expected_x=$1 expected_y=$2 expected_w=$3 expected_h=$4 X= Y= WIDTH= HEIGHT= _
  for _ in $(seq 1 60); do
    eval "$(window_geometry)"
    if [ "$X" = "$expected_x" ] && [ "$Y" = "$expected_y" ] && [ "$WIDTH" = "$expected_w" ] && [ "$HEIGHT" = "$expected_h" ]; then return 0; fi
    sleep .05
  done
  return 1
}
assert_window_geometry() {
  local label=$1 expected_x=$2 expected_y=$3 expected_w=$4 expected_h=$5 X= Y= WIDTH= HEIGHT=
  eval "$(window_geometry)"
  assert_eq "$label x" "$X" "$expected_x"; assert_eq "$label y" "$Y" "$expected_y"
  assert_eq "$label width" "$WIDTH" "$expected_w"; assert_eq "$label height" "$HEIGHT" "$expected_h"
}
window_map_state() { xwininfo -id "$1" 2>/dev/null | awk -F: '/Map State:/ { sub(/^[[:space:]]+/, "", $2); print $2 }'; }
wait_id_state() {
  local id=$1 expected=$2 actual= _
  for _ in $(seq 1 60); do actual=$(window_map_state "$id"); [ "$actual" = "$expected" ] && return 0; sleep .05; done
  return 1
}
assert_id_geometry() {
  local label=$1 id=$2 expected_x=$3 expected_y=$4 expected_w=$5 expected_h=$6 X= Y= WIDTH= HEIGHT=
  eval "$(xdotool getwindowgeometry --shell "$id" 2>/dev/null)"
  assert_eq "$label x" "$X" "$expected_x"; assert_eq "$label y" "$Y" "$expected_y"
  assert_eq "$label width" "$WIDTH" "$expected_w"; assert_eq "$label height" "$HEIGHT" "$expected_h"
}
pid_window_count() { xdotool search --pid "$DZEN_PID" 2>/dev/null | awk 'NF { count++ } END { print count + 0 }'; }
slave_window() { xdotool search --name '^dzen slave$' 2>/dev/null | tail -1; }
title_window() {
  local slave_hex
  slave_hex=$(printf '0x%x' "$1")
  xwininfo -id "$WIN" -children 2>/dev/null | awk -v slave="$slave_hex" '$1 ~ /^0x/ && tolower($1) != tolower(slave) { print $1; exit }'
}
start() {
  cleanup_dzen; WIN=
  printf 'xrandr title\nitem one\nitem two\n' | "$DZEN2_BINARY" "$@" -fn "$TEST_FONT" -bg "$BAR_BG" -fg "$BAR_FG" -p 30 >"$TMPDIR_XRANDR/dzen.stdout" 2>"$TMPDIR_XRANDR/dzen.stderr" &
  DZEN_PID=$!
  local _
  for _ in $(seq 1 60); do
    WIN=$(xdotool search --pid "$DZEN_PID" 2>/dev/null | head -1)
    [ -n "$WIN" ] && return 0
    kill -0 "$DZEN_PID" 2>/dev/null || return 1
    sleep .05
  done
  return 1
}
capture() {
  local name=$1 actual=$ACTUAL/$1.png expected=$EXPECTED/$1.png diff=$DIFFS/$1.png metric
  if ! import -window root "$actual" 2>/dev/null; then fail "capture $name.png"; return; fi
  if [ "${UPDATE_XRANDR_EXPECTED:-0}" = 1 ]; then cp "$actual" "$expected"; rm -f "$diff"; pass "updated expected $name.png"; return; fi
  if [ ! -f "$expected" ]; then fail "missing expected screenshot $expected (review actual, then run with UPDATE_XRANDR_EXPECTED=1)"; return; fi
  metric=$(compare -metric AE "$expected" "$actual" "$diff" 2>&1 || true); metric=${metric%% *}
  if [ "$metric" = 0 ]; then rm -f "$diff"; pass "screenshot $name.png"; else fail "screenshot $name differs by $metric pixels (see $diff)"; fi
}
run_rejected() {
  local label=$1; shift
  if printf 'test\n' | "$DZEN2_BINARY" "$@" -p 1 >"$TMPDIR_XRANDR/rejected.stdout" 2>"$TMPDIR_XRANDR/rejected.stderr"; then fail "$label accepted"; else pass "$label rejected"; fi
}

mapfile -t OUTPUTS < <(connected_outputs); mapfile -t ACTIVE_OUTPUTS < <(active_outputs)
if [ "${#ACTIVE_OUTPUTS[@]}" -eq 0 ]; then fail "dummy DDX has no connected output with active CRTC"; summary; exit 1; fi
OUTPUT=${ACTIVE_OUTPUTS[0]}; OUTPUT_COUNT=${#OUTPUTS[@]}; ACTIVE_COUNT=${#ACTIVE_OUTPUTS[@]}; SECONDARY= CONNECT_OUTPUT=
mapfile -t RANDR_OUTPUTS < <(randr_outputs)
if [ "${#RANDR_OUTPUTS[@]}" -ge 2 ]; then
  SECONDARY=${RANDR_OUTPUTS[1]}
  if ! xrandr --addmode "$SECONDARY" "${SCREEN_W}x${SCREEN_H}" >/dev/null 2>&1 ||
     ! xrandr --addmode "$SECONDARY" "${ALT_W}x${ALT_H}" >/dev/null 2>&1; then
    info "Second dummy output $SECONDARY cannot accept test modes; multi-output checks will skip"
    SECONDARY=
  fi
fi
if [ "${#RANDR_OUTPUTS[@]}" -ge 3 ]; then
  CONNECT_OUTPUT=${RANDR_OUTPUTS[2]}
  if ! xrandr --addmode "$CONNECT_OUTPUT" "${ALT_W}x${ALT_H}" >/dev/null 2>&1; then CONNECT_OUTPUT=; fi
fi
echo "XRandR backend: Xorg dummy on $DISPLAY"
echo "Connected outputs: $OUTPUT_COUNT; active CRTCs: $ACTIVE_COUNT; selected: $OUTPUT"
if [ -n "$SECONDARY" ]; then info "Prepared inactive secondary output: $SECONDARY"; fi
if [ -n "$CONNECT_OUTPUT" ]; then info "Prepared never-activated connection-test output: $CONNECT_OUTPUT"; fi
xrandr --query

test_case 01 "output discovery and selector validation"
LM_OUTPUT=$("$DZEN2_BINARY" -lm 2>&1)
assert_match "-lm lists active output $OUTPUT" "$LM_OUTPUT" "(^|[[:space:]])$OUTPUT([[:space:]]|$)"
run_rejected "unknown output" -output __dzen_nonexistent_output__
run_rejected "-output/-xs conflict (output first)" -output "$OUTPUT" -xs 1
run_rejected "-output/-xs conflict (xs first)" -xs 1 -output "$OUTPUT"
run_rejected "border missing argument" -b
run_rejected "border empty field" -b '1,,red'
run_rejected "border three-width form" -b '1,2,3'
run_rejected "border numeric overflow" -b '999999999999999999999999999999'
run_rejected "border invalid X11 color" -b '2,__not_an_x11_color__'

test_case 02 "initial output placement" 02-dummy-initial.png
if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -h "$BAR_H" -w "$BAR_W"; then
  wait_window_state IsViewable || fail "initial window is viewable"
  assert_window_geometry "initial placement" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
  INITIAL_WIN=$WIN; capture 02-dummy-initial
else fail "start on connected output"; INITIAL_WIN=; fi

test_case 03 "output mode change" 03-dummy-mode.png
if xrandr --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" >/dev/null 2>&1; then
  if wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0"; then
    assert_window_geometry "mode change" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
    assert_eq "mode change keeps title window" "$WIN" "$INITIAL_WIN"; capture 03-dummy-mode
  else fail "mode change event did not settle"; fi
else skip "dummy DDX rejected alternate mode ${ALT_W}x${ALT_H}"; fi

test_case 04 "output rotation" 04-dummy-rotation.png
if xrandr --output "$OUTPUT" --rotate left >/dev/null 2>&1; then
  if wait_output_geometry "$OUTPUT" "${ALT_H}x${ALT_W}+0+0"; then
    assert_window_geometry "left rotation" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
    assert_eq "rotation keeps title window" "$WIN" "$INITIAL_WIN"; capture 04-dummy-rotation
  else fail "rotation event did not settle"; fi
  xrandr --output "$OUTPUT" --rotate normal >/dev/null 2>&1 || fail "restore normal rotation"
  wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0" || fail "normal rotation did not settle"
else skip "dummy DDX does not support rotation"; fi

test_case 05 "extended desktop and output position" 05-dummy-extended-position.png
if [ -n "$SECONDARY" ]; then
  if xrandr --fb "$((2 * ALT_W))x${ALT_H}" \
      --output "$SECONDARY" --mode "${ALT_W}x${ALT_H}" --pos 0x0 \
      --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos "${ALT_W}x0" >/dev/null 2>&1; then
    wait_output_geometry "$SECONDARY" "${ALT_W}x${ALT_H}+0+0" || fail "secondary output position did not settle"
    wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+${ALT_W}+0" || fail "selected output position did not settle"
    wait_window_geometry "$((ALT_W + BAR_X))" "$BAR_Y" "$BAR_W" "$BAR_H" || fail "bar did not follow positioned output"
    assert_window_geometry "positioned output" "$((ALT_W + BAR_X))" "$BAR_Y" "$BAR_W" "$BAR_H"
    assert_eq "position change keeps title window" "$WIN" "$INITIAL_WIN"
    DUAL_LM_OUTPUT=$("$DZEN2_BINARY" -lm 2>&1)
    assert_match "-lm lists positioned output $OUTPUT" "$DUAL_LM_OUTPUT" "(^|[[:space:]])$OUTPUT([[:space:]]|$)"
    assert_match "-lm lists active secondary $SECONDARY" "$DUAL_LM_OUTPUT" "(^|[[:space:]])$SECONDARY([[:space:]]|$)"
    capture 05-dummy-extended-position
  else fail "configure extended dummy outputs"; fi
  xrandr --fb "${ALT_W}x${ALT_H}" --output "$SECONDARY" --off \
    --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore primary output after position test"
  wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0" || fail "primary output restoration did not settle"
else
  skip "dummy DDX exposes no secondary output for position testing"
fi

test_case 06 "output disconnect and reconnect" 06-dummy-reconnect.png
if xrandr --output "$OUTPUT" --off >/dev/null 2>&1; then
  if wait_window_state IsUnMapped; then pass "disconnect hides title window"; else fail "disconnect hides title window"; fi
  if xrandr --fb 1200x900 --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos 0x0 >/dev/null 2>&1; then
    wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0" || fail "reconnect output did not settle"
    if wait_window_state IsViewable; then pass "reconnect maps title window"; else fail "reconnect maps title window"; fi
    assert_window_geometry "reconnect restoration" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
    assert_eq "reconnect keeps title window" "$WIN" "$INITIAL_WIN"; capture 06-dummy-reconnect
  else fail "reconnect output"; fi
else skip "dummy DDX cannot disable its output"; fi

test_case 07 "repeated topology changes without coordinate drift"
for cycle in 1 2 3; do
  if [ $((cycle % 2)) -eq 0 ]; then CYCLE_W=$SCREEN_W; CYCLE_H=$SCREEN_H; else CYCLE_W=$ALT_W; CYCLE_H=$ALT_H; fi
  if ! xrandr --fb "${CYCLE_W}x${CYCLE_H}" --output "$OUTPUT" --mode "${CYCLE_W}x${CYCLE_H}" --rotate normal --pos 0x0 >/dev/null 2>&1; then fail "cycle $cycle topology command"; continue; fi
  if ! wait_output_geometry "$OUTPUT" "${CYCLE_W}x${CYCLE_H}+0+0"; then fail "cycle $cycle topology did not settle"; continue; fi
  assert_window_geometry "cycle $cycle no drift" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
  assert_eq "cycle $cycle keeps title window" "$WIN" "$INITIAL_WIN"
done

test_case 08 "negative output anchoring"
if start -output "$OUTPUT" -x -1 -y -1 -h "$BAR_H" -w "$BAR_W"; then
  CURRENT_GEOM=$(output_geometry "$OUTPUT"); CURRENT_W=${CURRENT_GEOM%%x*}; GEOM_REST=${CURRENT_GEOM#*x}
  CURRENT_H=${GEOM_REST%%+*}; CURRENT_POS=${CURRENT_GEOM#*+}; CURRENT_X=${CURRENT_POS%%+*}; CURRENT_Y=${CURRENT_POS#*+}
  assert_window_geometry "negative anchoring" "$((CURRENT_X + CURRENT_W - BAR_W))" "$((CURRENT_Y + CURRENT_H - BAR_H))" "$BAR_W" "$BAR_H"
else fail "start with negative anchoring"; fi

test_case 09 "dock properties across disconnect and reconnect"
if start -output "$OUTPUT" -dock -h "$BAR_H"; then
  DOCK_TYPE=$(xprop -id "$WIN" _NET_WM_WINDOW_TYPE 2>/dev/null || true); assert_match "dock window type" "$DOCK_TYPE" '_NET_WM_WINDOW_TYPE_DOCK'
  BEFORE_STRUT=$(xprop -id "$WIN" _NET_WM_STRUT_PARTIAL 2>/dev/null || true); assert_match "connected dock has nonzero strut" "$BEFORE_STRUT" '=[^\n]*[1-9][0-9]*'
  if xrandr --output "$OUTPUT" --off >/dev/null 2>&1; then
    wait_window_state IsUnMapped || fail "disconnect hides dock"
    OFF_STRUT=$(xprop -id "$WIN" _NET_WM_STRUT_PARTIAL 2>/dev/null || true)
    if [ -z "$OFF_STRUT" ] || printf '%s\n' "$OFF_STRUT" | grep -Eq 'not found|=([[:space:]]*0,){11}[[:space:]]*0$'; then pass "disconnect clears dock strut"; else fail "disconnect leaves dock strut: $OFF_STRUT"; fi
    xrandr --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos 0x0 >/dev/null 2>&1 || fail "reconnect dock output"
    wait_window_state IsViewable || fail "reconnect maps dock"
    AFTER_STRUT=$(xprop -id "$WIN" _NET_WM_STRUT_PARTIAL 2>/dev/null || true); assert_match "reconnect restores dock strut" "$AFTER_STRUT" '=[^\n]*[1-9][0-9]*'
  else skip "dummy DDX cannot disconnect output for dock test"; fi
else fail "start dock on output"; fi

# Restore a deterministic root/output geometry for legacy -xs and menu checks.
xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --rotate normal --pos 0x0 >/dev/null 2>&1 || fail "restore initial geometry"
wait_output_geometry "$OUTPUT" "${SCREEN_W}x${SCREEN_H}+0+0" || fail "initial geometry did not settle"

# Legacy Xinerama selector fallbacks (0, negative, and too large) all use the
# root geometry.  Unlike -output, a valid explicit -xs remains static.
test_case 10 "static legacy Xinerama selection and root fallback"
for XS_FALLBACK in 0 -3 99; do
  if start -xs "$XS_FALLBACK" -x -1 -y -1 -h "$BAR_H" -w "$BAR_W"; then
    assert_window_geometry "-xs $XS_FALLBACK root fallback" "$((SCREEN_W - BAR_W))" "$((SCREEN_H - BAR_H))" "$BAR_W" "$BAR_H"
  else fail "-xs $XS_FALLBACK root fallback starts"; fi
done
if start -xs 1 -x -1 -y -1 -h "$BAR_H" -w "$BAR_W"; then
  XS_STATIC_X=$((SCREEN_W - BAR_W)); XS_STATIC_Y=$((SCREEN_H - BAR_H))
  assert_window_geometry "valid -xs initial" "$XS_STATIC_X" "$XS_STATIC_Y" "$BAR_W" "$BAR_H"
  xrandr --fb "${ALT_W}x${ALT_H}" --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos 0x0 >/dev/null 2>&1 || fail "change geometry during static -xs"
  wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0" || fail "static -xs topology event did not settle"
  assert_window_geometry "valid -xs ignores RandR event" "$XS_STATIC_X" "$XS_STATIC_Y" "$BAR_W" "$BAR_H"
else fail "valid -xs 1 starts"; fi

xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore geometry for menu tests"
wait_output_geometry "$OUTPUT" "${SCREEN_W}x${SCREEN_H}+0+0" || fail "menu geometry did not settle"
SLAVE_W=$((BAR_W + 73)); SLAVE_X=$((BAR_X + (BAR_W - SLAVE_W) / 2))
test_case 11 "vertical menu layout" 11-dummy-vertical-menu.png
if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -tw "$BAR_W" -w "$SLAVE_W" -l 2 -m v \
    -e 'onstart=uncollapse;sigusr1=collapse;sigusr2=uncollapse' -h "$BAR_H"; then
  SLAVE_WIN=$(slave_window); TITLE_WIN=$(title_window "$SLAVE_WIN")
  assert_eq "vertical PID-associated top-level count" "$(pid_window_count)" 1
  assert_window_geometry "vertical expanded outer" "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$((3 * BAR_H))"
  assert_id_geometry "vertical title child" "$TITLE_WIN" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
  assert_id_geometry "vertical slave child" "$SLAVE_WIN" "$SLAVE_X" "$((BAR_Y + BAR_H))" "$SLAVE_W" "$((2 * BAR_H))"
  if kill -USR1 "$DZEN_PID" && wait_window_geometry "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"; then
    pass "collapse shrinks outer to title"
  else fail "collapse shrinks outer to title"; fi
  if wait_id_state "$SLAVE_WIN" IsUnMapped; then pass "collapse unmaps slave child"; else fail "collapse unmaps slave child"; fi
  if kill -USR2 "$DZEN_PID" && wait_window_geometry "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$((3 * BAR_H))"; then
    pass "uncollapse expands outer to union"
  else fail "uncollapse expands outer to union"; fi
  if wait_id_state "$SLAVE_WIN" IsViewable; then pass "uncollapse maps slave child"; else fail "uncollapse maps slave child"; fi
  capture 11-dummy-vertical-menu
else fail "vertical menu starts"; fi
test_case 12 "horizontal menu layout" 12-dummy-horizontal-menu.png
if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -tw "$BAR_W" -w "$SLAVE_W" -l 2 -m h \
    -e 'onstart=uncollapse;sigusr1=hide;sigusr2=unhide' -h "$BAR_H"; then
  SLAVE_WIN=$(slave_window); TITLE_WIN=$(title_window "$SLAVE_WIN")
  assert_eq "horizontal PID-associated top-level count" "$(pid_window_count)" 1
  assert_window_geometry "horizontal outer wraps slave" "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$BAR_H"
  assert_id_geometry "horizontal title child" "$TITLE_WIN" "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"
  assert_id_geometry "horizontal slave child" "$SLAVE_WIN" "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$BAR_H"
  if wait_id_state "$TITLE_WIN" IsUnMapped; then pass "horizontal title child remains unmapped"; else fail "horizontal title child remains unmapped"; fi
  capture 12-dummy-horizontal-menu
  if kill -USR1 "$DZEN_PID" && wait_window_geometry "$SLAVE_X" "$BAR_Y" "$SLAVE_W" 1; then
    pass "horizontal hide shrinks outer"
  else fail "horizontal hide shrinks outer"; fi
  if kill -USR2 "$DZEN_PID" && wait_window_geometry "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$BAR_H"; then
    pass "horizontal unhide restores outer"
  else fail "horizontal unhide restores outer"; fi
else fail "horizontal menu starts"; fi

test_case 12a "pointer transitions across outer children"
xdotool mousemove "$((SCREEN_W - 10))" "$((SCREEN_H - 10))"
if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -tw "$BAR_W" -w "$SLAVE_W" -l 2 -m v -h "$BAR_H"; then
  xdotool mousemove "$((BAR_X + 10))" "$((BAR_Y + 10))"
  if wait_window_geometry "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$((3 * BAR_H))"; then
    pass "entering title uncollapses through child event"
  else fail "entering title uncollapses through child event"; fi
  xdotool mousemove "$((SLAVE_X + 10))" "$((BAR_Y + BAR_H + 10))"
  if wait_window_geometry "$SLAVE_X" "$BAR_Y" "$SLAVE_W" "$((3 * BAR_H))"; then
    pass "title-to-slave transition stays expanded"
  else fail "title-to-slave transition stays expanded"; fi
  xdotool mousemove "$((SLAVE_X + SLAVE_W + 20))" "$((BAR_Y + 10))"
  if wait_window_geometry "$BAR_X" "$BAR_Y" "$BAR_W" "$BAR_H"; then
    pass "leaving slave collapses without outer crossing action"
  else fail "leaving slave collapses without outer crossing action"; fi
else fail "pointer-transition menu starts"; fi

test_case 12b "slave-name compatibility and outer metadata"
if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -w "$BAR_W" -l 1 -slave-name stage2-slave -dock -h "$BAR_H"; then
  NAMED_SLAVE=$(xdotool search --name '^stage2-slave$' 2>/dev/null | tail -1)
  if [ -n "$NAMED_SLAVE" ]; then pass "-slave-name finds internal slave child"; else fail "-slave-name finds internal slave child"; fi
  assert_eq "metadata PID-associated window count" "$(pid_window_count)" 1
  OUTER_METADATA=$(xprop -id "$WIN" WM_CLASS WM_NAME _NET_WM_PID _NET_WM_WINDOW_TYPE 2>/dev/null || true)
  assert_match "outer owns WM class" "$OUTER_METADATA" 'WM_CLASS.*dzen2.*dzen'
  assert_match "outer owns title name" "$OUTER_METADATA" 'WM_NAME.*dzen title'
  assert_match "outer owns PID" "$OUTER_METADATA" '_NET_WM_PID.*[0-9]'
  assert_match "outer owns dock type" "$OUTER_METADATA" '_NET_WM_WINDOW_TYPE_DOCK'
  CHILD_PID=$(xprop -id "$NAMED_SLAVE" _NET_WM_PID 2>/dev/null || true)
  assert_match "slave child has no PID metadata" "$CHILD_PID" 'not found'
else fail "slave-name metadata menu starts"; fi

# With no monitor selector, dzen follows the root framebuffer. Expanding the
# framebuffer must resize the existing full-width title without recreating it.
test_case 13 "root framebuffer expansion" "13a-dummy-root-initial.png, 13b-dummy-root-expanded.png"
if start -y "$BAR_Y" -h "$BAR_H"; then
  ROOT_WIN=$WIN
  assert_window_geometry "root initial" 0 "$BAR_Y" "$SCREEN_W" "$BAR_H"
  capture 13a-dummy-root-initial
  if xrandr --fb "${EXPANDED_W}x${EXPANDED_H}" --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1; then
    wait_root_geometry "${EXPANDED_W}x${EXPANDED_H}" || fail "expanded root geometry did not settle"
    wait_window_geometry 0 "$BAR_Y" "$EXPANDED_W" "$BAR_H" || fail "root expansion did not resize title"
    assert_window_geometry "root expansion" 0 "$BAR_Y" "$EXPANDED_W" "$BAR_H"
    assert_eq "root expansion keeps title window" "$WIN" "$ROOT_WIN"
    capture 13b-dummy-root-expanded
  else fail "expand root framebuffer"; fi
else fail "start on root geometry"; fi
xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore root framebuffer"
wait_root_geometry "${SCREEN_W}x${SCREEN_H}" || fail "restored root geometry did not settle"

# Starting on a known but currently disconnected output must create hidden
# windows and remain alive until the output returns.
test_case 14 "startup on an RR_Disconnected output, then connect it" 14-dummy-output-connected.png
if [ -n "$CONNECT_OUTPUT" ]; then
  assert_eq "test output initial RandR connection" "$(output_connection "$CONNECT_OUTPUT")" disconnected
fi
if [ -n "$CONNECT_OUTPUT" ] && start -output "$CONNECT_OUTPUT" -x "$BAR_X" -y "$BAR_Y" -h "$BAR_H" -w "$BAR_W"; then
  if wait_window_state IsUnMapped; then pass "known disconnected output starts hidden"; else fail "known disconnected output starts hidden"; fi
  if xrandr --fb "$((SCREEN_W + ALT_W))x${SCREEN_H}" \
      --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 \
      --output "$CONNECT_OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos "${SCREEN_W}x0" >/dev/null 2>&1; then
    wait_output_geometry "$CONNECT_OUTPUT" "${ALT_W}x${ALT_H}+${SCREEN_W}+0" || fail "test output connection did not settle"
    assert_eq "test output active RandR connection" "$(output_connection "$CONNECT_OUTPUT")" connected
    if wait_window_state IsViewable; then pass "disconnected-start window maps after reconnect"; else fail "disconnected-start window maps after reconnect"; fi
    assert_window_geometry "connected output offsets" "$((SCREEN_W + BAR_X))" "$BAR_Y" "$BAR_W" "$BAR_H"
    capture 14-dummy-output-connected
    xrandr --output "$CONNECT_OUTPUT" --off >/dev/null 2>&1 || fail "disable test output after startup test"
    if wait_window_state IsUnMapped; then pass "output without CRTC hides title window"; else fail "output without CRTC hides title window"; fi
    if [ -z "$(output_geometry "$CONNECT_OUTPUT")" ]; then pass "test output final CRTC is inactive"; else fail "test output final CRTC remains active"; fi
  else fail "reconnect after disconnected startup"; fi
elif [ -z "$CONNECT_OUTPUT" ]; then
  skip "dummy DDX exposes no untouched output for connection testing"
else fail "known disconnected output process remains alive"; fi
if [ -n "$CONNECT_OUTPUT" ]; then
  xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$CONNECT_OUTPUT" --off \
    --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore primary after secondary connection test"
fi
wait_output_geometry "$OUTPUT" "${SCREEN_W}x${SCREEN_H}+0+0" || fail "primary restoration after connection test did not settle"

test_case 15 "unrelated output topology event"
if [ -n "$SECONDARY" ]; then
  if ! xrandr --fb "$((SCREEN_W + ALT_W))x${SCREEN_H}" \
      --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 \
      --output "$SECONDARY" --mode "${ALT_W}x${ALT_H}" --pos "${SCREEN_W}x0" >/dev/null 2>&1; then
    fail "activate secondary for unrelated-output test"
  fi
  wait_output_geometry "$SECONDARY" "${ALT_W}x${ALT_H}+${SCREEN_W}+0" || fail "secondary activation did not settle"
  if start -output "$OUTPUT" -x "$BAR_X" -y "$BAR_Y" -h "$BAR_H" -w "$BAR_W"; then
    BEFORE=$(window_geometry); BEFORE_WIN=$WIN
    if xrandr --fb "$((2 * SCREEN_W))x${SCREEN_H}" \
        --output "$SECONDARY" --mode "${SCREEN_W}x${SCREEN_H}" --pos "${SCREEN_W}x0" >/dev/null 2>&1; then
      wait_output_geometry "$SECONDARY" "${SCREEN_W}x${SCREEN_H}+${SCREEN_W}+0" || fail "unrelated mode change did not settle"
      AFTER=$(window_geometry)
      assert_eq "unrelated-output event leaves layout unchanged" "$AFTER" "$BEFORE"
      assert_eq "unrelated-output event keeps title window" "$WIN" "$BEFORE_WIN"
    else fail "change unrelated output mode"; fi
  else fail "start for unrelated-output event"; fi
else
  skip "dummy DDX exposes no secondary output for unrelated-output testing"
fi
if [ -n "$SECONDARY" ]; then
  xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$SECONDARY" --off \
    --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore primary after unrelated-output test"
fi

wait_output_geometry "$OUTPUT" "${SCREEN_W}x${SCREEN_H}+0+0" || fail "border-case output restoration did not settle"
test_case 16 "border clamping and state across disconnect/reconnect"
if start -output "$OUTPUT" -x -1 -y -1 -tw "$BAR_W" -w "$SLAVE_W" -l 2 -m v \
    -b '3,7,9,11,#406080' -e 'onstart=uncollapse;sigusr1=hide;sigusr2=unhide' -h "$BAR_H"; then
  BORDER_X=$((SCREEN_W - SLAVE_W - 11 - 7)); BORDER_Y=$((SCREEN_H - 3 * BAR_H - 3 - 9))
  BORDER_W=$((SLAVE_W + 11 + 7)); BORDER_H=$((3 * BAR_H + 3 + 9))
  BORDER_WIN=$WIN; BORDER_SLAVE=$(slave_window); BORDER_TITLE=$(title_window "$BORDER_SLAVE")
  assert_window_geometry "bordered negative-anchor clamp" "$BORDER_X" "$BORDER_Y" "$BORDER_W" "$BORDER_H"
  assert_id_geometry "bordered title child" "$BORDER_TITLE" "$((SCREEN_W - BAR_W - 7))" \
    "$((SCREEN_H - BAR_H - 9))" "$BAR_W" "$BAR_H"
  if kill -USR1 "$DZEN_PID" && wait_window_geometry "$BORDER_X" "$BORDER_Y" "$BORDER_W" "$BORDER_H"; then
    pass "expanded vertical hide preserves bordered outer"
  else fail "expanded vertical hide preserves bordered outer"; fi
  if wait_id_state "$BORDER_TITLE" IsUnMapped; then pass "expanded vertical hide unmaps title child"; else fail "expanded vertical hide unmaps title child"; fi
  assert_id_geometry "hidden bordered title child" "$BORDER_TITLE" "$((SCREEN_W - BAR_W - 7))" \
    "$((SCREEN_H - BAR_H - 9))" "$BAR_W" 1
  if xrandr --output "$OUTPUT" --off >/dev/null 2>&1; then
    wait_window_state IsUnMapped || fail "bordered hidden surface disconnects"
    if xrandr --fb "${ALT_W}x${ALT_H}" --output "$OUTPUT" --mode "${ALT_W}x${ALT_H}" --pos 0x0 >/dev/null 2>&1; then
      wait_output_geometry "$OUTPUT" "${ALT_W}x${ALT_H}+0+0" || fail "bordered reconnect output did not settle"
      if wait_window_state IsViewable; then pass "bordered hidden surface reconnects"; else fail "bordered hidden surface reconnects"; fi
      assert_eq "bordered reconnect keeps outer window" "$WIN" "$BORDER_WIN"
      assert_window_geometry "bordered reconnect clamp" "$((ALT_W - BORDER_W))" "$BORDER_Y" "$BORDER_W" "$BORDER_H"
      assert_id_geometry "bordered reconnect keeps hidden title" "$BORDER_TITLE" "$((ALT_W - BAR_W - 7))" \
        "$((ALT_H - BAR_H - 9))" "$BAR_W" 1
      if wait_id_state "$BORDER_TITLE" IsUnMapped; then pass "bordered reconnect keeps title hidden"; else fail "bordered reconnect keeps title hidden"; fi
      if wait_id_state "$BORDER_SLAVE" IsViewable; then pass "bordered reconnect keeps expanded slave"; else fail "bordered reconnect keeps expanded slave"; fi
      kill -USR2 "$DZEN_PID" || fail "unhide bordered title after reconnect"
    else fail "reconnect bordered output"; fi
  else skip "dummy DDX cannot disconnect output for border state test"; fi
else fail "bordered menu starts"; fi

xrandr --fb "${SCREEN_W}x${SCREEN_H}" --output "$OUTPUT" --mode "${SCREEN_W}x${SCREEN_H}" --pos 0x0 >/dev/null 2>&1 || fail "restore output for bordered dock"
wait_output_geometry "$OUTPUT" "${SCREEN_W}x${SCREEN_H}+0+0" || fail "bordered dock output did not settle"
test_case 16b "dock strut includes static borders"
if start -output "$OUTPUT" -dock -y 0 -h "$BAR_H" -b '3,7,9,11,#406080'; then
  BORDER_STRUT=$(xprop -id "$WIN" _NET_WM_STRUT_PARTIAL 2>/dev/null || true)
  assert_match "bordered dock exact collapsed strut" "$BORDER_STRUT" \
    '= 0, 0, 40, 0, 0, 0, 0, 0, 0, 1023, 0, 0$'
else fail "bordered dock starts"; fi

cleanup_dzen; summary; exit "$FAILURES"
