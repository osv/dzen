#!/usr/bin/env bash
# Capture the selected, already validated examples into a staging directory.
set -euo pipefail

if [[ $# != 5 ]]; then
    echo "usage: $0 SOURCE_ROOT DZEN2_BINARY OUTPUT_DIRECTORY INPUT_DIRECTORY MANIFEST" >&2
    exit 2
fi
source_root=$(cd "$1" && pwd)
binary=$(realpath "$2")
mkdir -p "$3"
output=$(cd "$3" && pwd)
inputs=$(realpath "$4")
manifest=$(realpath "$5")
for tool in awk Xvfb xset xdotool xwd; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
if command -v magick >/dev/null; then
    convert=(magick)
else
    command -v convert >/dev/null || { echo "Missing ImageMagick" >&2; exit 1; }
    convert=(convert)
fi
[[ -x "$binary" ]] || { echo "Build dzen2 first: $binary" >&2; exit 1; }
work=$(mktemp -d)
app_pid=
xvfb_pid=
cleanup() {
    if [[ -n "$app_pid" ]]; then kill "$app_pid" 2>/dev/null || true; wait "$app_pid" 2>/dev/null || true; fi
    if [[ -n "$xvfb_pid" ]]; then kill "$xvfb_pid" 2>/dev/null || true; wait "$xvfb_pid" 2>/dev/null || true; fi
    rm -rf -- "$work"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# Let Xvfb reserve its display atomically. No desktop resources are imported.
Xvfb -displayfd 3 -noreset -screen 0 1200x900x24 -nolisten tcp 3>"$work/display" >"$work/xvfb.log" 2>&1 &
xvfb_pid=$!
for ((attempt=0; attempt<100; attempt++)); do
    [[ -s "$work/display" ]] && break
    kill -0 "$xvfb_pid" 2>/dev/null || {
        echo 'Xvfb failed to start (check permission to create X11 sockets).' >&2
        tail -n 12 "$work/xvfb.log" >&2
        exit 1
    }
    sleep .05
done
[[ -s "$work/display" ]] || { echo "Xvfb startup timed out" >&2; exit 1; }
export DISPLAY=":$(<"$work/display")"
xset q >/dev/null
xdotool mousemove 1199 899
cd "$source_root"
while IFS= read -r ident; do
    [[ -n "$ident" ]] || continue
    count=$(wc -l < "$inputs/$ident.txt")
    { printf '\n'; cat "$inputs/$ident.txt"; } > "$work/input"
    "$binary" -p -l "$count" -w 800 -tw 800 -x 30 -y 30 -sa l \
        -bg '#111111' -fg grey70 -b '1,#5FBF77' -pad '5,20' \
        -title-name "dzen-doc-$ident" \
        -e 'onstart=uncollapse,hide;onnewinput=scrollhome' \
        < "$work/input" > "$work/app.log" 2>&1 &
    app_pid=$!
    window=
    for ((attempt=0; attempt<100; attempt++)); do
        window=$(xdotool search --onlyvisible --name "^dzen-doc-$ident$" 2>/dev/null | head -n 1) || true
        [[ -n "$window" ]] && break
        kill -0 "$app_pid" 2>/dev/null || { cat "$work/app.log" >&2; exit 1; }
        sleep .05
    done
    [[ -n "$window" ]] || { echo "Window timed out: $ident" >&2; exit 1; }
    # Mapping precedes stdin processing; allow rendering to settle, then capture twice.
    sleep .2
    xwd -silent -id "$window" | "${convert[@]}" xwd:- "$work/first.png"
    sleep .1
    xwd -silent -id "$window" | "${convert[@]}" xwd:- "$output/$ident.png"
    "${convert[@]}" "$work/first.png" "$output/$ident.png" -compose difference -composite \
        -format '%[fx:maxima]' info: | { read -r difference || true; [[ "$difference" == 0 ]]; } || {
        echo "Unstable rendering: $ident" >&2; exit 1;
    }
    kill "$app_pid"
    wait "$app_pid" || [[ $? == 143 ]]
    app_pid=
    echo "Captured $output/$ident.png"
done < "$manifest"
