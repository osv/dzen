#!/usr/bin/env bash

if { [ -t 1 ] || [ "${GITHUB_ACTIONS:-}" = true ]; } && [ -z "${NO_COLOR:-}" ]; then
  RED=$'\033[31m'
  GREEN=$'\033[32m'
  YELLOW=$'\033[33m'
  GRAY=$'\033[90m'
  RESET=$'\033[0m'
else
  RED=
  GREEN=
  YELLOW=
  GRAY=
  RESET=
fi
NC=$RESET

test_announce() {
  local line

  for line in "$@"; do
    printf '%s%s%s\n' "$GRAY" "$line" "$RESET"
  done
}

test_project_path() {
  local path=$1

  case $path in
    "$SOURCE_ROOT"/*) printf '%s\n' "${path#"$SOURCE_ROOT"/}" ;;
    "$BUILD_ROOT"/*) printf '%s\n' "${path#"$BUILD_ROOT"/}" ;;
    *) printf '%s\n' "$path" ;;
  esac
}

test_require_commands() {
  local command missing=0

  for command in "$@"; do
    if ! command -v "$command" >/dev/null 2>&1; then
      printf '%sFAIL:%s required command is unavailable: %s\n' "$RED" "$RESET" "$command" >&2
      missing=1
    fi
  done
  return "$missing"
}

# Print the absolute error (AE) pixel count reported by ImageMagick.  HDRI
# builds print a normalized value followed by the absolute count in
# parentheses; non-HDRI builds print only the absolute count.
test_parse_ae_metric() {
  local output=$1

  if [[ $output =~ ^[[:space:]]*([0-9]+)[[:space:]]*$ ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
    return 0
  fi
  if [[ $output =~ ^[[:space:]]*[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?[[:space:]]*\(([0-9]+)\)[[:space:]]*$ ]]; then
    printf '%s\n' "${BASH_REMATCH[3]}"
    return 0
  fi
  return 1
}

test_find_free_display() {
  local display_number

  for display_number in $(seq "${1:-200}" "${2:-599}"); do
    if [ ! -S "/tmp/.X11-unix/X$display_number" ] && [ ! -e "/tmp/.X$display_number-lock" ]; then
      printf ':%s\n' "$display_number"
      return 0
    fi
  done
  return 1
}
