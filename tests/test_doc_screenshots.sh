#!/usr/bin/env bash
# Cache/selection tests use a fake renderer; no X server or real PNGs are needed.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
. "$root/tests/integration/test_common.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/source/build-aux" "$work/source/docs/screenshots"
cp "$root/build-aux/generate-docs.awk" "$work/source/build-aux/"
cat > "$work/source/build-aux/capture-doc-screenshots.sh" <<'RENDERER'
#!/usr/bin/env bash
set -euo pipefail
cat "$5" >> "$1/captured"
[[ ! -f "$1/fail" ]] || exit 1
while IFS= read -r ident; do
    # A deterministic stand-in for an image records exactly what was rendered.
    cp "$4/$ident.txt" "$3/$ident.png"
done < "$5"
RENDERER
cat > "$work/source/README.dzen" <<'BEFORE'
Example: first — First

Input:
    ^^fg(red)Red

Result:
    ^fg(red)Red

Example: second — Second

Input:
    Plain

Result:
    Plain

BEFORE
passes=0
check() {
    if "$@"; then
        printf '%sPASS:%s %s\n' "$GREEN" "$RESET" "$name"
        passes=$((passes + 1))
    else
        printf '%sFAIL:%s %s\n' "$RED" "$RESET" "$name" >&2
        exit 1
    fi
}
update() {
    bash "$root/build-aux/doc-screenshots.sh" "$work/source" /bin/true "${1:-$work/source/docs/screenshots}"
}
expect_captured() {
    cat > "$work/expected"
    name=$1
    check diff -u "$work/expected" "$work/source/captured"
}
test_announce 'INFO: Incremental documentation screenshots test plan:' \
    '      - render new/changed/missing images; skip unchanged inputs and prose edits' \
    '      - reuse distributed assets, retain cache on failure, remove deleted examples'
update
expect_captured 'new examples are rendered' <<'AFTER'
first
second
AFTER
: > "$work/source/captured"
# Failure marker proves that a no-op never even starts the renderer.
touch "$work/source/fail"
update
expect_captured 'unchanged examples skip rendering' <<'AFTER'
AFTER
printf '\nAn extra paragraph.\n' >> "$work/source/README.dzen"
update
expect_captured 'prose-only changes skip rendering' <<'AFTER'
AFTER
update "$work/out-of-tree"
expect_captured 'out-of-tree build reuses distributed images' <<'AFTER'
AFTER
name='out-of-tree PNG matches the distributed PNG'
check cmp "$work/source/docs/screenshots/first.png" "$work/out-of-tree/first.png"
rm "$work/source/fail"
sed -i 's/red/blue/g' "$work/source/README.dzen"
update
expect_captured 'only the changed example is rendered' <<'AFTER'
first
AFTER
cat > "$work/expected" <<'AFTER'
^fg(blue)Red
AFTER
name='updated cache stores the decoded input'
check diff -u "$work/expected" "$work/source/docs/screenshots/first.txt"
: > "$work/source/captured"
rm "$work/source/docs/screenshots/second.png"
update
expect_captured 'missing PNG is regenerated' <<'AFTER'
second
AFTER
: > "$work/source/captured"
cp "$work/source/docs/screenshots/first.png" "$work/old.png"
cp "$work/source/docs/screenshots/first.txt" "$work/old.txt"
sed -i 's/blue/green/g' "$work/source/README.dzen"
touch "$work/source/fail"
name='renderer failure propagates to make'
if update; then check false; else check true; fi
name='failed capture keeps the old PNG'
check cmp "$work/old.png" "$work/source/docs/screenshots/first.png"
name='failed capture keeps the old input cache'
check cmp "$work/old.txt" "$work/source/docs/screenshots/first.txt"
rm "$work/source/fail"
: > "$work/source/captured"
update
expect_captured 'failed capture is retried' <<'AFTER'
first
AFTER
# After removing an example, only its manifest-owned assets may be deleted.
sed -i '/^Example: second/,$d' "$work/source/README.dzen"
touch "$work/source/docs/screenshots/unrelated.png"
update
name='removed example loses its PNG and cache'
check test ! -e "$work/source/docs/screenshots/second.png"
check test ! -e "$work/source/docs/screenshots/second.txt"
name='unrelated files are preserved'
check test -f "$work/source/docs/screenshots/unrelated.png"
cat > "$work/expected" <<'AFTER'
first
AFTER
name='manifest follows the current examples'
check diff -u "$work/expected" "$work/source/docs/screenshots/manifest"
printf '%sPASS: %d%s  %sFAIL: 0%s\n' "$GREEN" "$passes" "$RESET" "$RED" "$RESET"
