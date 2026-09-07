#!/usr/bin/env bash
# Quoted HEREDOCs keep the before/after text literal, including shell substitutions.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
# shellcheck source=integration/test_common.sh
. "$root/tests/integration/test_common.sh"
test_require_commands awk cat diff mktemp sed
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
processor=$root/build-aux/generate-docs.awk
passes=0
failures=0

pass() {
    printf '%sPASS:%s %s\n' "$GREEN" "$RESET" "$1"
    passes=$((passes + 1))
}
fail() {
    printf '%sFAIL:%s %s\n' "$RED" "$RESET" "$1" >&2
    failures=$((failures + 1))
}
compare() {
    local name=$1 expected=$2 actual=$3
    if diff -u --label expected --label actual "$expected" "$actual" > "$work/diff"; then
        pass "$name"
    else
        fail "$name"
        cat "$work/diff" >&2
    fi
}
run() { awk -v mode="$1" -v outdir="$work" -f "$processor" "$2"; }
check_render() {
    local mode=$1
    if run "$mode" "$work/before" > "$work/actual" 2> "$work/error"; then
        compare "$mode: references, headings, code blocks and literal carets" "$work/after" "$work/actual"
    else
        fail "$mode: generator exited unsuccessfully"
        cat "$work/error" >&2
    fi
}

test_announce \
    'INFO: Documentation generator test plan:' \
    '      - compare literal before/after HEREDOCs for dzen, Markdown, man and examples' \
    '      - validate repository examples and reject malformed examples'

# Before: one source exercises live/displayed commands, compound arguments,
# existing code spans, both fence styles and real shell substitutions.
cat > "$work/before" <<'BEFORE'
SYNOPSIS
========

    `dzen2` `-fg COLOR` `-p`
    echo `date` $(date)

Live ^foo(bar), shown ^^foo(bar), ^^foo(), ^^p(10;20) and ^^ca(1,echo hello).
Carets: ^^ ^^^^ ^^^^foo(bar) ^^^^^^foo(bar)
Use `^^foo(bar)` and `-foo bar`, `-underline N[,COLOR]`, `-geometry WxH+X+Y`, `-ta l|c|r`.
Keep ``a ` ^^foo(bar)`` intact.

```sh
`-foo bar` ^^foo(bar) `date` $(date)
Example: not-an-example
```

~~~text
`-ta l|c|r` ^^foo()
~~~
BEFORE

# After: markdown output.
cat > "$work/after" <<'AFTER'
## SYNOPSIS

    dzen2 -fg COLOR -p
    echo `date` $(date)

Live ^foo(bar), shown `^foo(bar)`, `^foo()`, `^p(10;20)` and `^ca(1,echo hello)`.
Carets: ^ ^^ ^^foo(bar) ^^^foo(bar)
Use `^foo(bar)` and `-foo bar`, `-underline N[,COLOR]`, `-geometry WxH+X+Y`, `-ta l|c|r`.
Keep ``a ` ^foo(bar)`` intact.

```sh
-foo bar ^foo(bar) `date` $(date)
Example: not-an-example
```

~~~text
-ta l|c|r ^foo()
~~~
AFTER
check_render markdown

# After: man output.
cat > "$work/after" <<'AFTER'
# SYNOPSIS

    _DZENFMTBOLDTOKENdzen2_DZENFMTRESETTOKEN _DZENFMTBOLDTOKEN-fg_DZENFMTRESETTOKEN _DZENFMTITALICTOKENCOLOR_DZENFMTRESETTOKEN _DZENFMTBOLDTOKEN-p_DZENFMTRESETTOKEN
    echo `date` $(date)

Live ^foo(bar), shown _DZENFMTBOLDTOKEN^foo(_DZENFMTRESETTOKEN_DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN, _DZENFMTBOLDTOKEN^foo()_DZENFMTRESETTOKEN, _DZENFMTBOLDTOKEN^p(_DZENFMTRESETTOKEN_DZENFMTITALICTOKEN10;20_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN and _DZENFMTBOLDTOKEN^ca(_DZENFMTRESETTOKEN_DZENFMTITALICTOKEN1,echo hello_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN.
Carets: ^ ^^ ^^foo(bar) ^^^foo(bar)
Use _DZENFMTBOLDTOKEN^foo(_DZENFMTRESETTOKEN_DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN and _DZENFMTBOLDTOKEN-foo_DZENFMTRESETTOKEN _DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN, _DZENFMTBOLDTOKEN-underline_DZENFMTRESETTOKEN _DZENFMTITALICTOKENN[,COLOR]_DZENFMTRESETTOKEN, _DZENFMTBOLDTOKEN-geometry_DZENFMTRESETTOKEN _DZENFMTITALICTOKENWxH+X+Y_DZENFMTRESETTOKEN, _DZENFMTBOLDTOKEN-ta_DZENFMTRESETTOKEN _DZENFMTITALICTOKENl|c|r_DZENFMTRESETTOKEN.
Keep a ` _DZENFMTBOLDTOKEN^foo(_DZENFMTRESETTOKEN_DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN intact.

```sh
_DZENFMTBOLDTOKEN-foo_DZENFMTRESETTOKEN _DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN _DZENFMTBOLDTOKEN^foo(_DZENFMTRESETTOKEN_DZENFMTITALICTOKENbar_DZENFMTRESETTOKEN_DZENFMTBOLDTOKEN)_DZENFMTRESETTOKEN `date` $(date)
Example: not-an-example
```

~~~text
_DZENFMTBOLDTOKEN-ta_DZENFMTRESETTOKEN _DZENFMTITALICTOKENl|c|r_DZENFMTRESETTOKEN _DZENFMTBOLDTOKEN^foo()_DZENFMTRESETTOKEN
~~~
AFTER
check_render man

# After: dzen output.
cat > "$work/after" <<'AFTER'
^fg(#6fbf47)SYNOPSIS^fg()
^fg(#6fbf47)========^fg()

    ^fg(lightblue)dzen2^fg() ^fg(lightblue)-fg^fg() ^underline()COLOR^underline(off) ^fg(lightblue)-p^fg()
    echo `date` $(date)

Live ^foo(bar), shown ^fg(lightblue)^^foo(^fg()^underline()bar^underline(off)^fg(lightblue))^fg(), ^fg(lightblue)^^foo()^fg(), ^fg(lightblue)^^p(^fg()^underline()10;20^underline(off)^fg(lightblue))^fg() and ^fg(lightblue)^^ca(^fg()^underline()1,echo hello^underline(off)^fg(lightblue))^fg().
Carets: ^^ ^^^^ ^^^^foo(bar) ^^^^^^foo(bar)
Use ^fg(lightblue)^^foo(^fg()^underline()bar^underline(off)^fg(lightblue))^fg() and ^fg(lightblue)-foo^fg() ^underline()bar^underline(off), ^fg(lightblue)-underline^fg() ^underline()N[,COLOR]^underline(off), ^fg(lightblue)-geometry^fg() ^underline()WxH+X+Y^underline(off), ^fg(lightblue)-ta^fg() ^underline()l|c|r^underline(off).
Keep a ` ^fg(lightblue)^^foo(^fg()^underline()bar^underline(off)^fg(lightblue))^fg() intact.

```sh
^fg(lightblue)-foo^fg() ^underline()bar^underline(off) ^fg(lightblue)^^foo(^fg()^underline()bar^underline(off)^fg(lightblue))^fg() `date` $(date)
Example: not-an-example
```

~~~text
^fg(lightblue)-ta^fg() ^underline()l|c|r^underline(off) ^fg(lightblue)^^foo()^fg()
~~~
AFTER
check_render dzen

# Before: Input doubles the carets of the exact live Result below.
cat > "$work/valid" <<'BEFORE'
Example: colors — Colors

Input:
    ^^fg(red)Red^^fg() ^^^^

Result:
    ^fg(red)Red^fg() ^^

BEFORE
# After: extraction writes the live Result, without structural indentation.
cat > "$work/after" <<'AFTER'
^fg(red)Red^fg() ^^
AFTER
cat > "$work/expected-manifest" <<'AFTER'
colors
AFTER
if run examples "$work/valid" 2> "$work/error"; then
    compare 'examples: decoded Input equals unchanged Result' "$work/after" "$work/colors.txt"
    compare 'examples: manifest preserves the identifier' "$work/expected-manifest" "$work/manifest"
else
    fail 'examples: extraction exited unsuccessfully'
    cat "$work/error" >&2
fi

# The repository examples must remain structurally valid as new examples are added.
if run examples "$root/README.dzen" 2> "$work/error"; then
    pass "repository examples: valid Input/Result pairs"
else
    fail "repository examples: extraction exited unsuccessfully"
    cat "$work/error" >&2
fi

# Each malformed variant must fail for the intended reason in all four modes.
for mutation in duplicate missing empty mismatch live control id unterminated; do
    case $mutation in
        duplicate)
            cat "$work/valid" "$work/valid" > "$work/bad"
            reason='duplicate example: colors' ;;
        missing)
            sed 's/Result:/Missing:/' "$work/valid" > "$work/bad"
            reason='colors: expected Result:' ;;
        empty)
            sed '/^    \^fg/d' "$work/valid" > "$work/bad"
            reason='colors: empty or unterminated Result' ;;
        mismatch)
            sed 's/^    \^fg(red)/    wrong/' "$work/valid" > "$work/bad"
            reason='colors: Input must decode to Result exactly' ;;
        live)
            sed 's/\^\^fg(red)/^fg(red)/' "$work/valid" > "$work/bad"
            reason='every caret in Input must be doubled' ;;
        control)
            sed 's/fg(red)/hide()/g' "$work/valid" > "$work/bad"
            reason='window control is not allowed in Result' ;;
        id)
            sed 's/colors —/..\/colors —/' "$work/valid" > "$work/bad"
            reason='invalid Example heading' ;;
        unterminated)
            sed '$aFollowing' "$work/valid" | sed '/^$/d' > "$work/bad"
            reason='colors: empty or unterminated Input' ;;
    esac
    for mode in dzen markdown man examples; do
        if run "$mode" "$work/bad" > "$work/actual" 2> "$work/error"; then
            fail "$mode: accepted malformed example ($mutation)"
        elif [[ $(< "$work/error") == "$work/bad: $reason" ]]; then
            pass "$mode: rejected $mutation"
        else
            fail "$mode: unexpected diagnostic for $mutation (expected: $reason)"
            cat "$work/error" >&2
        fi
    done
done
printf '%sPASS: %d%s  %sFAIL: %d%s\n' "$GREEN" "$passes" "$RESET" "$RED" "$failures" "$RESET"
[[ $failures == 0 ]]
