#!/usr/bin/env bash
set -u

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SOURCE_ROOT=$(cd "${DZEN2_TEST_SOURCE_ROOT:-$SCRIPT_DIR/../..}" && pwd)
BUILD_ROOT=$(cd "${DZEN2_TEST_BUILD_ROOT:-$SOURCE_ROOT}" && pwd)
# shellcheck source=test_common.sh
. "$SOURCE_ROOT/tests/integration/test_common.sh"

failures=0

expect_metric() {
  local input=$1 expected=$2 actual
  if actual=$(test_parse_ae_metric "$input") && [ "$actual" = "$expected" ]; then
    printf 'PASS: AE metric %q -> %s\n' "$input" "$actual"
  else
    printf 'FAIL: AE metric %q expected %s, got %s\n' "$input" "$expected" "${actual:-error}" >&2
    failures=$((failures + 1))
  fi
}

expect_rejected() {
  local input=$1 actual
  if actual=$(test_parse_ae_metric "$input"); then
    printf 'FAIL: malformed AE metric %q was accepted as %s\n' "$input" "$actual" >&2
    failures=$((failures + 1))
  else
    printf 'PASS: rejected malformed AE metric %q\n' "$input"
  fi
}

expect_metric '0' 0
expect_metric '123' 123
expect_metric '65535 (1)' 1
expect_metric '1.5e+09 (23037)' 23037
expect_rejected ''
expect_rejected 'compare: image widths or heights differ'
expect_rejected '12 pixels'
expect_rejected '65535 (one)'

exit "$failures"
