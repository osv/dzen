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
