#!/usr/bin/env bash

SCRIPT_DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SOURCE_ROOT=$(cd "${DZEN2_TEST_SOURCE_ROOT:-$SCRIPT_DIR/../../..}" && pwd)
BUILD_ROOT=$(cd "${DZEN2_TEST_BUILD_ROOT:-$SOURCE_ROOT}" && pwd)
DZEN2_BINARY=${DZEN2_TEST_BINARY:-$BUILD_ROOT/src/dzen2}
# shellcheck source=../../test_common.sh
. "$SOURCE_ROOT/tests/integration/test_common.sh"
cd "$SOURCE_ROOT"

# Global variables
USE_VIRTUAL_DISPLAY=true
XVFB_PID=""
SHOW_IMAGES_ON_SUCCESS=false
DISABLE_IMAGES=false
IS_KITTY_TERMINAL=false
ORIGINAL_DISPLAY="${DISPLAY:-}"

# ImageMagick command variables (set by detect_imagemagick)
CONVERT_CMD=""
COMPARE_CMD=""

# Function to show help
show_help() {
    cat << EOF
Usage: $0 [OPTIONS] <test_file>[:<line_number>]

OPTIONS:
    --help          Show this help message
    --native        Run tests on native X11 display (current DISPLAY)
    --virtual       Run tests in virtual Xvfb display (default)
    --show-images   Display actual images on success (Kitty terminal only)
    --no-images     Disable image display even in Kitty terminal

EXAMPLES:
    $0 tests/integration/visual/xft/cases.md
    $0 --native tests/integration/visual/xft/cases.md
    $0 tests/integration/visual/xft/cases.md:286
    $0 --show-images tests/integration/visual/xft/cases.md

NOTE: By default, tests run in a virtual Xvfb display for isolation.
      Use --native to run tests on your current X11 display.
      In Kitty terminal, error images are shown by default; use --no-images to disable.
      Image display in Kitty works best with --native flag when running from a graphical session.
EOF
}

# Detect ImageMagick version and set appropriate commands
detect_imagemagick() {
    # Check if magick command is available (ImageMagick 7)
    if command -v magick >/dev/null 2>&1; then
        # Check if it's actually ImageMagick 7 by looking at version
        local version_output=$(magick -version 2>/dev/null | head -1)
        if echo "$version_output" | grep -q "ImageMagick 7"; then
            CONVERT_CMD="magick"
            COMPARE_CMD="magick compare"
            return 0
        fi
    fi
    
    # Fall back to ImageMagick 6 commands
    if command -v convert >/dev/null 2>&1 && command -v compare >/dev/null 2>&1; then
        CONVERT_CMD="convert"
        COMPARE_CMD="compare"
        return 0
    fi
    
    echo "Error: Neither ImageMagick 6 (convert/compare) nor ImageMagick 7 (magick) found"
    exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --help)
            show_help
            exit 0
            ;;
        --native)
            USE_VIRTUAL_DISPLAY=false
            shift
            ;;
        --virtual)
            USE_VIRTUAL_DISPLAY=true
            shift
            ;;
        --show-images)
            SHOW_IMAGES_ON_SUCCESS=true
            shift
            ;;
        --no-images)
            DISABLE_IMAGES=true
            shift
            ;;
        --)
            shift
            break
            ;;
        -*)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage information." >&2
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

# Check for test file argument
if [ $# -ne 1 ]; then
    echo "Error: Missing test file argument" >&2
    echo "Use --help for usage information." >&2
    exit 1
fi

# Parse test file and optional line number
if [[ "$1" =~ ^([^:]+):([0-9]+)$ ]]; then
    TEST_FILE="${BASH_REMATCH[1]}"
    TARGET_LINE="${BASH_REMATCH[2]}"
else
    TEST_FILE="$1"
    TARGET_LINE=""
fi

# Check if test file exists
if [ ! -f "$TEST_FILE" ]; then
    echo "Error: Test file '$TEST_FILE' not found"
    exit 1
fi
TEST_FILE=$(readlink -f "$TEST_FILE")

# Read reference images from the source tree and put generated images in the
# corresponding build-tree directory (the same place for in-tree builds).
SCREENSHOT_DIR=$(dirname "$TEST_FILE")
RELATIVE_SCREENSHOT_DIR=${SCREENSHOT_DIR#"$SOURCE_ROOT"/}
EXPECTED_DIR="$SCREENSHOT_DIR/expected"
ACTUAL_DIR="$BUILD_ROOT/$RELATIVE_SCREENSHOT_DIR/actual"
DIFF_DIR="$BUILD_ROOT/$RELATIVE_SCREENSHOT_DIR/diffs"

if [ "$USE_VIRTUAL_DISPLAY" = true ]; then
    DISPLAY_DESCRIPTION='an isolated Xvfb display'
else
    DISPLAY_DESCRIPTION="the native display ${DISPLAY:-<unset>}"
fi
test_announce \
    'INFO: Visual integration test plan:' \
    "      - run cases from $(test_project_path "$TEST_FILE") on $DISPLAY_DESCRIPTION" \
    '      - drive dzen with xdotool and capture each requested screenshot' \
    "      - compare captures with $(test_project_path "$EXPECTED_DIR")" \
    "      - write captures and differences below $(test_project_path "$ACTUAL_DIR") and $(test_project_path "$DIFF_DIR")"
if [ "${UPDATE_VISUAL_EXPECTED:-0}" = 1 ]; then
    test_announce '      - UPDATE_VISUAL_EXPECTED=1: replace expected screenshots with captures'
fi

# Create directories
mkdir -p "$EXPECTED_DIR"
mkdir -p "$ACTUAL_DIR"
mkdir -p "$DIFF_DIR"

# Detect if running in Kitty terminal
if [[ "$TERM" == "xterm-kitty" ]] || [[ -n "$KITTY_WINDOW_ID" ]]; then
    IS_KITTY_TERMINAL=true
fi

# Function to start virtual display
start_virtual_display() {
    local display_number
    local server_display
    local -a transport

    server_display=$(test_find_free_display 99 599) || {
        echo "No free X display found" >&2
        exit 1
    }
    display_number=${server_display#:}
    if [ "$(stat -c %u /tmp/.X11-unix 2>/dev/null || printf 1)" = 0 ]; then
        transport=(-nolisten tcp)
        DISPLAY=$server_display
    else
        transport=(-nolisten unix -nolisten local -listen tcp)
        DISPLAY=127.0.0.1:$display_number
    fi
    
    echo "Starting virtual display ${server_display}..."
    Xvfb "$server_display" -screen 0 1920x1080x24 "${transport[@]}" > /dev/null 2>&1 &
    XVFB_PID=$!

    export DISPLAY
    for _ in $(seq 1 100); do
        xset q >/dev/null 2>&1 && break
        kill -0 "$XVFB_PID" 2>/dev/null || break
        sleep .05
    done
    xset q >/dev/null 2>&1 || { echo "Xvfb failed to start" >&2; exit 1; }
    echo "Virtual display started on DISPLAY=${DISPLAY}"
}

# Global variable to track if we're interrupted
INTERRUPTED=false

# Function to cleanup virtual display
cleanup_virtual_display() {
    if [ -n "$XVFB_PID" ]; then
        echo "Stopping virtual display (PID: $XVFB_PID)..."
        kill "$XVFB_PID" 2>/dev/null || true
        wait "$XVFB_PID" 2>/dev/null || true
        XVFB_PID=""
    fi
}

# Function to handle interruption
handle_interrupt() {
    INTERRUPTED=true
    echo -e "\n${RED}Interrupted! Cleaning up...${NC}"
    
    # Kill any running dzen2 processes started by this script
    if [ -n "${program_coproc_PID:-}" ]; then
        kill "${program_coproc_PID}" 2>/dev/null || true
    fi
    
    # Clean up virtual display
    cleanup_virtual_display
    
    echo -e "${RED}Tests interrupted by user.${NC}"
    exit 130  # Standard exit code for SIGINT
}

# Set up signal handlers for cleanup
trap handle_interrupt INT TERM
trap cleanup_virtual_display EXIT

# Detect ImageMagick and set command variables
detect_imagemagick

# Setup display
if [ "$USE_VIRTUAL_DISPLAY" = true ]; then
    test_require_commands xwd xdotool xwininfo xprop base64 Xvfb xset seq stat || exit 1
    start_virtual_display
else
    test_require_commands xwd xdotool xwininfo xprop base64 xset || exit 1
    [ -n "${DISPLAY:-}" ] || { echo "DISPLAY is not set" >&2; exit 1; }
    xset q >/dev/null 2>&1 || { echo "Cannot connect to DISPLAY=$DISPLAY" >&2; exit 1; }
    echo "Running tests on native X11 display: $DISPLAY"
fi

[ -x "$DZEN2_BINARY" ] || { echo "dzen2 test binary is not built: $DZEN2_BINARY" >&2; exit 1; }

# Threshold for acceptable difference in screenshots
delta_threshold=2
all_tests_passed=true

# Function to display image in Kitty terminal
display_image_in_kitty() {
    local image_path=$1
    local label=$2

    # Only display if in Kitty terminal and images are not disabled
    if [ "$IS_KITTY_TERMINAL" = true ] && [ "$DISABLE_IMAGES" = false ] && [ -f "$image_path" ]; then
        # Check if ORIGINAL_DISPLAY is valid
        if [ -z "$ORIGINAL_DISPLAY" ]; then
            # Only show warning once
            if [ "${KITTY_IMAGE_WARNING_SHOWN:-false}" = false ]; then
                echo "  Note: Kitty image display requires a valid DISPLAY. Use --native flag for best results."
                KITTY_IMAGE_WARNING_SHOWN=true
            fi
            return
        fi

        # Try to display image with error handling
        # Use --stdin=no to prevent kitten icat from reading stdin (which would hang the script)
        if DISPLAY="$ORIGINAL_DISPLAY" kitten icat --stdin=no --align left "$image_path" 2>/dev/null; then
            if [ -n "$label" ]; then
                echo -e "  └──[ $label ]"
            fi
        else
            # Only show error once
            if [ "${KITTY_IMAGE_ERROR_SHOWN:-false}" = false ]; then
                echo "  Note: Unable to display images in Kitty. Use --native flag or ensure Kitty terminal is accessible."
                KITTY_IMAGE_ERROR_SHOWN=true
            fi
        fi
    fi
}

run_test() {
  local test_name=$1
  local cmd_args=$2
  shift 2
  local steps=("$@")

  # Check if we've been interrupted
  if [ "$INTERRUPTED" = true ]; then
    return
  fi

  echo -n "Running test: $test_name ... "

  # Run the app using coproc and capture the PID
  eval "args=($cmd_args)"
  local app_output_path="$ACTUAL_DIR/app_output.txt"
  coproc program_coproc { exec "$DZEN2_BINARY" -p "${args[@]}" &> "$app_output_path"; }
  local app_pid=$program_coproc_PID

  # Wait for the app to start (adjust this time if necessary)
  sleep 0.6  # Adjust this as needed based on your app's loading time

  # Find the window ID associated with the app's PID
  local window_id=$(xdotool search --pid "$app_pid" | head -n 1)
  if [ -z "$window_id" ]; then
    echo -e "${RED}Error: Could not find window for PID $app_pid.${NC}"
    all_tests_passed=false
    # Close the coprocess
    kill "$app_pid" 2>/dev/null || true
    # Check if file descriptors are valid before closing
    if [ -n "${program_coproc[1]}" ] && [ "${program_coproc[1]}" -gt 0 ] 2>/dev/null; then
      exec {program_coproc[1]}>&- 2>/dev/null || true
    fi
    if [ -n "${program_coproc[0]}" ] && [ "${program_coproc[0]}" -gt 0 ] 2>/dev/null; then
      exec {program_coproc[0]}>&- 2>/dev/null || true
    fi
    return
  fi

  local crop_geometry=""
  local expected_screenshot=""
  local check_num=0

  check_geometry() {
    local id=$1 expected=$2 label=$3 geometry actual
    geometry=$(xdotool getwindowgeometry --shell "$id")
    actual=$(printf '%s\n' "$geometry" | awk -F= '/^(X|Y|WIDTH|HEIGHT)=/ { value[$1]=$2 } END { print value["X"] "," value["Y"] "," value["WIDTH"] "," value["HEIGHT"] }')
    check_num=$((check_num + 1))
    if [ "$actual" = "$expected" ]; then
      echo -en "$check_num: $label geometry ${GREEN}Pass. ${NC}"
    else
      echo -e "\n${RED}$check_num: $label geometry: expected $expected, got $actual.${NC}"
      all_tests_passed=false
    fi
  }

  check_mapping() {
    local id=$1 expected=$2 label=$3 actual
    actual=$(xwininfo -id "$id" 2>/dev/null | awk -F: '/Map State:/ { sub(/^[[:space:]]+/, "", $2); print $2 }')
    check_num=$((check_num + 1))
    if [ "$actual" = "$expected" ]; then
      echo -en "$check_num: $label mapping ${GREEN}Pass. ${NC}"
    else
      echo -e "\n${RED}$check_num: $label mapping: expected $expected, got $actual.${NC}"
      all_tests_passed=false
    fi
  }

  content_window_id() {
    local kind=$1 child name
    while read -r child; do
      [ -n "$child" ] || continue
      name=$(xprop -id "$child" WM_NAME 2>/dev/null || true)
      if [ "$kind" = slave ] && [[ $name == *'dzen slave'* ]]; then echo "$child"; return; fi
      if [ "$kind" = title ] && [[ $name != *'dzen slave'* ]]; then echo "$child"; return; fi
    done < <(xwininfo -id "$window_id" -children | awk '/^[[:space:]]+0x[0-9a-f]+ / { print $1 }')
  }

  for step in "${steps[@]}"; do
    IFS='|' read -r action params <<< "$step"
    case "$action" in
      'geometry')
        check_geometry "$window_id" "$params" Outer
        ;;
      'title_geometry')
        local title_id
        title_id=$(content_window_id title)
        check_geometry "$title_id" "$params" Title
        ;;
      'slave_geometry')
        local slave_id
        slave_id=$(content_window_id slave)
        check_geometry "$slave_id" "$params" Slave
        ;;
      'outer_mapping')
        check_mapping "$window_id" "$params" Outer
        ;;
      'title_mapping')
        local title_id
        title_id=$(content_window_id title)
        check_mapping "$title_id" "$params" Title
        ;;
      'slave_mapping')
        local slave_id
        slave_id=$(content_window_id slave)
        check_mapping "$slave_id" "$params" Slave
        ;;
      'click')
        xdotool click "$params"
        sleep 0.1
        ;;
      'mouse')
        IFS=',' read -r x y <<< "$params"
        xdotool mousemove --window "$window_id" "$x" "$y"
        sleep 0.1
        ;;
      'presskey')
        IFS=',' read -r key <<< "$params"
        xdotool key "$key"
        sleep 0.1
        ;;
      'click_and_check')
        check_num=$((check_num + 1))
        button=$(echo "$params" | cut -d',' -f1)
        expected_output=$(echo "$params" | cut -d',' -f2- | xargs)
        xdotool click "$button"
        sleep 0.1
        app_output=$(cat "$app_output_path")
        if [[ "$app_output" != *"$expected_output"* ]]; then
          echo -e "\n${RED}$check_num: Error:\nExpected output: '${expected_output}'.${NC}"
          echo -e "Actual output: '${app_output}'.\n"
          all_tests_passed=false
        else
          echo -en "$check_num: Click button ${button} and check output ${GREEN}Pass. ${NC}"
        fi
        ;;
      'check_no_output')
        check_num=$((check_num + 1))
        app_output=$(cat "$app_output_path")
        if [ -z "$app_output" ]; then
          echo -en "$check_num: No action output ${GREEN}Pass. ${NC}"
        else
          echo -e "\n${RED}$check_num: Expected no action output, got '${app_output}'.${NC}"
          all_tests_passed=false
        fi
        ;;
      'crop')
        crop_geometry="$params"
        ;;
      'screenshot')
        # Get the window's geometry (position and size)
        local window_geometry
        window_geometry=$(xdotool getwindowgeometry --shell "$window_id")
        local window_x
        local window_y
        local window_width
        local window_height
        window_x=$(echo "$window_geometry" | grep 'X=' | cut -d '=' -f 2)
        window_y=$(echo "$window_geometry" | grep 'Y=' | cut -d '=' -f 2)
        window_width=$(echo "$window_geometry" | grep 'WIDTH=' | cut -d '=' -f 2)
        window_height=$(echo "$window_geometry" | grep 'HEIGHT=' | cut -d '=' -f 2)

        check_num=$((check_num + 1))
        expected_screenshot="$params"
        if [[ "$expected_screenshot" = "" ]]; then
          echo -e "\n${RED}Subtest: $check_num: Error:\n${RED}You forgot to set image in markdown:${NC}"
          echo -e "${RED}![reference](./expected/$test_name.png)${NC}\n"
          all_tests_passed=false
          return
        fi
        # Extract just the filename from the path
        local screenshot_filename=$(basename "$expected_screenshot")
        local actual_screenshot_path="$ACTUAL_DIR/$screenshot_filename"
        local expected_screenshot_path="$EXPECTED_DIR/$screenshot_filename"
        # Take screenshot
        if [ -n "$crop_geometry" ]; then
          xwd -root | $CONVERT_CMD "xwd:-" -crop "$crop_geometry" +repage "$actual_screenshot_path"
        else
          xwd -root | $CONVERT_CMD "xwd:-" -crop "${window_width}x${window_height}+${window_x}+${window_y}" +repage "$actual_screenshot_path"
        fi
        if [ "${UPDATE_VISUAL_EXPECTED:-0}" = 1 ]; then
          cp "$actual_screenshot_path" "$expected_screenshot_path"
          rm -f "$DIFF_DIR/$screenshot_filename"
          echo -en "$check_num: Screenshot ${GREEN}Updated.${NC} "
        # Compare with the expected screenshot
        elif [ -f "$expected_screenshot_path" ]; then
          local diff_screenshot="$DIFF_DIR/${screenshot_filename}"
          local raw_diff compare_status diff
          raw_diff=$($COMPARE_CMD -metric AE -fuzz 5% \
            "$actual_screenshot_path" "$expected_screenshot_path" "$diff_screenshot" 2>&1)
          compare_status=$?
          if (( compare_status > 1 )) || ! diff=$(test_parse_ae_metric "$raw_diff"); then
            echo -e "\n${RED}Subtest: $check_num: Error:\n${RED}ImageMagick comparison failed" \
              "for \"$test_name\" (status $compare_status).${NC}"
            echo -e "${RED} Output: ${raw_diff:-<empty>}${NC}\n"
            all_tests_passed=false
          elif (( (compare_status == 0 && diff != 0) || (compare_status == 1 && diff == 0) )); then
            echo -e "\n${RED}Subtest: $check_num: Error:\n${RED}ImageMagick returned inconsistent" \
              "status and AE metric ($compare_status, $diff).${NC}\n"
            all_tests_passed=false
          elif (( diff > delta_threshold )); then
            echo -e "\n${RED}Subtest: $check_num: Error:\n${RED}Difference in \"$test_name\" exceeds threshold! ($diff)${NC}"
            echo -e "${RED} See ./${diff_screenshot}${NC}\n"
            # Display images in Kitty terminal on error
            display_image_in_kitty "$expected_screenshot_path" "Expected: $expected_screenshot_path"
            display_image_in_kitty "$actual_screenshot_path" "Actual: $actual_screenshot_path"
            display_image_in_kitty "$diff_screenshot" "Difference: $diff_screenshot"
            all_tests_passed=false
          else
            rm -f "$diff_screenshot"
            echo -en "$check_num: Scr ${GREEN}Pass. ${NC}"
            # Display actual image on success if requested
            if [ "$SHOW_IMAGES_ON_SUCCESS" = true ]; then
              echo ""
              display_image_in_kitty "$actual_screenshot_path" "${GREEN}Actual (success)${NC}"
            fi
          fi
        else
          echo -e "\n${RED}Subtest: $check_num: Error: Expected screenshot not found for $test_name at $expected_screenshot_path.${NC}"
          echo -e "${YELLOW}Run with UPDATE_VISUAL_EXPECTED=1 after reviewing the actual image.${NC}\n"
          display_image_in_kitty "$actual_screenshot_path" "Actual: $actual_screenshot_path"
          all_tests_passed=false
        fi
        ;;
      'pipe_data')
        # Decode the base64-encoded pipe data
        decoded_pipe_data=$(echo "$params" | base64 --decode)
        # Send the pipe data to the program's stdin
        echo -e "$decoded_pipe_data" >&"${program_coproc[1]}"
        sleep 0.1
        ;;

      'sleep')
        echo -en "Sleep for ${params}s, "
        sleep "$params"
        ;;

      'dzen_running')
        check_num=$((check_num + 1))
        if [ "$params" = "yes" ]; then
          if kill -0 "$app_pid" 2>/dev/null; then
            echo -en "$check_num: Dzen running? yes ${GREEN}Pass. ${NC}"
          else
            echo -e "\n${RED}$check_num: Error:\nExpected app to be running, but it is not (PID: $app_pid).${NC}"
            all_tests_passed=false
          fi
        else  # "no"
          if kill -0 "$app_pid" 2>/dev/null; then
            echo -e "\n${RED}$check_num: Error:\nExpected app NOT to be running, but it's alive (PID: $app_pid).${NC}"
            all_tests_passed=false
          else
            echo -en "$check_num: Dzen running? no ${GREEN}Pass. ${NC}"
          fi
        fi
        ;;
    esac
  done
  echo

  if kill -0 "$app_pid" 2>/dev/null; then
    # Close the coprocess
    kill "$app_pid" 2>/dev/null || true
    # Check if file descriptors are valid before closing
    if [ -n "${program_coproc[1]}" ] && [ "${program_coproc[1]}" -gt 0 ] 2>/dev/null; then
      exec {program_coproc[1]}>&- 2>/dev/null || true
    fi
    if [ -n "${program_coproc[0]}" ] && [ "${program_coproc[0]}" -gt 0 ] 2>/dev/null; then
      exec {program_coproc[0]}>&- 2>/dev/null || true
    fi
  fi
}

# Function to parse the test cases from markdown
run_tests() {
  local test_file=$1
  local test_name=""
  local cmd_args=""
  local pipe_data_block=""
  local steps=()
  local in_pipe_data=false
  local line_number=0
  local current_test_start=0
  local target_test_name=""
  local target_test_start=0

  # First pass: find which test contains the target line
  if [ -n "$TARGET_LINE" ]; then
    local temp_line_num=0
    local temp_test_name=""
    local temp_test_start=0
    
    while IFS= read -r line; do
      ((temp_line_num++))
      if [[ $line == '## Test: '* ]]; then
        # Check if target line was in the previous test
        if [ -n "$temp_test_name" ] && [ "$TARGET_LINE" -ge "$temp_test_start" ] && [ "$TARGET_LINE" -lt "$temp_line_num" ]; then
          target_test_name="$temp_test_name"
          target_test_start="$temp_test_start"
          break
        fi
        temp_test_name="${line#'## Test: '}"
        temp_test_start=$temp_line_num
      fi
    done < "$test_file"
    
    # Check if target line is in the last test
    if [ -z "$target_test_name" ] && [ -n "$temp_test_name" ] && [ "$TARGET_LINE" -ge "$temp_test_start" ]; then
      target_test_name="$temp_test_name"
      target_test_start="$temp_test_start"
    fi
    
    if [ -z "$target_test_name" ]; then
      echo "Error: No test found at line $TARGET_LINE in $test_file"
      exit 1
    fi
  fi

  # Second pass: parse and run tests
  line_number=0
  while IFS= read -r line || [[ -n "$line" ]]; do
    # Check if we've been interrupted
    if [ "$INTERRUPTED" = true ]; then
      return
    fi
    
    ((line_number++))
    case $line in
      '## Test: '*)
        if [ -n "$test_name" ]; then
          # Run test if it's the target test or if no specific test was requested
          if [ -z "$TARGET_LINE" ] || [ "$test_name" = "$target_test_name" ]; then
            run_test "$test_name" "$cmd_args" "${steps[@]}"
            # If we just ran the target test, exit
            if [ -n "$TARGET_LINE" ]; then
              return
            fi
          fi
          # Reset variables
          test_name=""
          cmd_args=""
          pipe_data_block=""
          steps=()
          in_pipe_data=false
        fi
        test_name="${line#'## Test: '}"
        current_test_start=$line_number
        ;;
      '### Args: '*)
        cmd_args="${line#'### Args: '}"
        ;;
      '### Pipe data')
        pipe_data_block=""
        ;;
      '```')
        if $in_pipe_data; then
          # End of pipe data block
          in_pipe_data=false
          # Encode the pipe data to base64 to handle multi-line strings
          encoded_pipe_data=$(echo -n "$pipe_data_block" | base64 -w 0)
          steps+=("pipe_data|$encoded_pipe_data")
        else
          if [[ $pipe_data_block = '' ]]; then
            in_pipe_data=true
          fi
        fi
        ;;
      '### Mouse: '*)
        steps+=("mouse|${line#'### Mouse: '}")
        ;;
      '### Click: '*)
        steps+=("click|${line#'### Click: '}")
        ;;
      '### Geometry: '*)
        steps+=("geometry|${line#'### Geometry: '}")
        ;;
      '### Title geometry: '*)
        steps+=("title_geometry|${line#'### Title geometry: '}")
        ;;
      '### Slave geometry: '*)
        steps+=("slave_geometry|${line#'### Slave geometry: '}")
        ;;
      '### Outer mapping: '*)
        steps+=("outer_mapping|${line#'### Outer mapping: '}")
        ;;
      '### Title mapping: '*)
        steps+=("title_mapping|${line#'### Title mapping: '}")
        ;;
      '### Slave mapping: '*)
        steps+=("slave_mapping|${line#'### Slave mapping: '}")
        ;;
      '### Press key: '*)
        steps+=("presskey|${line#'### Press key: '}")
        ;;
      '### Click and check output: '*)
        steps+=("click_and_check|${line#'### Click and check output: '}")
        ;;
      '### Check no output')
        steps+=("check_no_output|")
        ;;
      '![reference](./'*)
        expected_screenshot="${line#'![reference](./'}"
        expected_screenshot="${expected_screenshot%')'}"
        steps+=("screenshot|$expected_screenshot")
        ;;
      '### Crop: '*)
        steps+=("crop|${line#'### Crop: '}")
        ;;

      '### Sleep: '*)
        steps+=("sleep|${line#'### Sleep: '}")
        ;;

      '### Dzen app is running?: '*)
        steps+=("dzen_running|${line#'### Dzen app is running?: '}")
        ;;

      *)
        # Collect lines for pipe_data if we are in a pipe_data block
        if $in_pipe_data; then
          pipe_data_block+="$line"$'\n'
        fi
        ;;
    esac
  done < "$test_file"

  # Run the last test if file ended without a new test block
  if [ -n "$test_name" ]; then
    if [ -z "$TARGET_LINE" ] || [ "$test_name" = "$target_test_name" ]; then
      run_test "$test_name" "$cmd_args" "${steps[@]}"
    fi
  fi
}

# Parse the test cases from the markdown file
run_tests "$TEST_FILE"

# Final result
echo "---------------------"
if [ "$all_tests_passed" = false ]; then
  echo -e "${RED}One or more tests failed.${NC}"
  exit 1
else
  echo -e "${GREEN}All tests passed.${NC}"
fi
