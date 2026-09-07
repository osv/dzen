#!/usr/bin/env bash
# Update published screenshots only when their decoded input changes or PNG is missing.
set -euo pipefail
if [[ $# != 3 ]]; then
    echo "usage: $0 SOURCE_ROOT DZEN2_BINARY OUTPUT_DIRECTORY" >&2
    exit 2
fi
source_root=$(cd "$1" && pwd)
binary=$(realpath "$2")
mkdir -p "$3"
output=$(cd "$3" && pwd)
# Stage on the destination filesystem so publishing a completed PNG is a rename.
work=$(mktemp -d "$output/.update.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir "$work/text" "$work/images"
awk -v mode=examples -v outdir="$work/text" \
    -f "$source_root/build-aux/generate-docs.awk" "$source_root/README.dzen"
declare -A current=()
: > "$work/pending"
while IFS= read -r ident; do
    current[$ident]=1
    if [[ -s "$output/$ident.png" ]] && cmp -s "$work/text/$ident.txt" "$output/$ident.txt"; then
        continue
    fi
    # Out-of-tree builds can reuse the distributed PNG and its matching input.
    if [[ "$output" != "$source_root/docs/screenshots" && ! -e "$output/$ident.txt" &&
          -s "$source_root/docs/screenshots/$ident.png" ]] &&
       cmp -s "$work/text/$ident.txt" "$source_root/docs/screenshots/$ident.txt"; then
        cp "$source_root/docs/screenshots/$ident.png" "$output/$ident.png"
        cp "$work/text/$ident.txt" "$output/$ident.txt"
    else
        printf '%s\n' "$ident" >> "$work/pending"
    fi
done < "$work/text/manifest"
if [[ -s "$work/pending" ]]; then
    # No display tools are needed until there is actually an image to render.
    bash "$source_root/build-aux/capture-doc-screenshots.sh" \
        "$source_root" "$binary" "$work/images" "$work/text" "$work/pending"
    while IFS= read -r ident; do
        mv "$work/images/$ident.png" "$output/$ident.png"
        mv "$work/text/$ident.txt" "$output/$ident.txt"
        echo "Updated $output/$ident.png"
    done < "$work/pending"
fi
# Remove only assets owned by the previous manifest, never arbitrary PNGs.
if [[ -f "$output/manifest" ]]; then
    while IFS= read -r ident; do
        [[ $ident =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo 'Invalid screenshot manifest' >&2; exit 1; }
        if [[ ! ${current[$ident]+present} ]]; then
            rm -f -- "$output/$ident.png" "$output/$ident.txt"
        fi
    done < "$output/manifest"
fi
if ! cmp -s "$work/text/manifest" "$output/manifest"; then
    mv "$work/text/manifest" "$output/manifest"
fi
