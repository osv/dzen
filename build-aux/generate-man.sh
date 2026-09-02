#!/bin/sh

set -eu

if test "$#" -ne 6; then
    echo "usage: $0 INPUT OUTPUT TITLE VERSION SECTION AUTHORS" >&2
    exit 2
fi

input=$1
output=$2
title=$3
version=$4
section=$5
authors=$6

raw="$output.raw"
tmp="$output.tmp"

cleanup() {
    rm -f "$raw" "$tmp"
}

trap cleanup 0
trap 'exit 1' 1 2 3 15

pandoc "$input" \
    --metadata "author=$authors" \
    --metadata "version=$version" \
    --metadata "title=$title" \
    --metadata "section=$section" \
    -s -t man -o "$raw"
sed 's/[[:blank:]]*$//' "$raw" > "$tmp"

if test -r "$output" && cmp -s "$tmp" "$output"; then
    touch "$output"
else
    mv -f "$tmp" "$output"
fi
