# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Initial setup (only needed once or after configure.ac changes)
autoreconf -vfi

# Configure with all features enabled
./configure --enable-xft --enable-xpm --enable-xinerama --enable-xcursor

# Build
make

# Run tests
make test

# Install (optional)
sudo make install

# Clean build
make clean
make distclean  # also removes configure-generated files
```

## Project Structure

```
.
├── src/               # Main dzen2 source code
│   ├── main.c        # Entry point and argument parsing
│   ├── draw.c        # Drawing functions and in-text command parsing
│   ├── action.c      # Event handling and action execution
│   ├── caches.c      # Performance optimizations (color/font caching)
│   ├── font.c        # Font management module
│   ├── util.c        # Utility functions
│   └── kvstore.c     # Key-value store for clickable areas
├── gadgets/          # Auxiliary tools
│   ├── dbar.c        # Progress bar library
│   ├── gdbar.c       # Graphical progress bar
│   ├── gcpubar.c     # CPU usage bar
│   └── textwidth.c   # Text width calculator
├── test_e2e          # Integration test runner
├── test_perfomance   # Performance and memory test tool
└── test_xinerama     # Multi-monitor test script
```

## Command Line Options

Below are list of options for `./src/dzen2`

- `-p [seconds]` - Persist for N seconds or indefinitely if no argument
- `-m [v|h]` - Menu mode: vertical (default) or horizontal
- `-l <lines>` - Number of lines in slave window (enables slave window)
- `-fn <font>` - Font specification (XFT or X11 font)
- `-bg <color>` - Background color
- `-fg <color>` - Foreground color
- `-x/-y <pixel>` - Window position
- `-w/-h <pixel>` - Window width/height
- `-ta <l|c|r>` - Title window text alignment (left/center/right)
- `-sa <l|c|r>` - Slave window text alignment
- `-xs <screen>` - Xinerama screen number (for multi-monitor)
- `-e <string>` - Event actions (e.g., "onstart=uncollapse")
- `-u` - Update continuously on stdin
- `-expand <left|right>` - Direction to expand menu
- `-dock` - Set `_NET_WM_WINDOW_TYPE_DOCK` property
- `-v` - Print version and enabled features

### Examples

```bash
# Basic usage
echo "Hello World" | ./src/dzen2 -p

# Menu with 5 lines
(echo "Title"; seq 1 5) | ./src/dzen2 -l 5 -p

# Positioned window with custom colors
echo "Status" | ./src/dzen2 -x 100 -y 50 -w 200 -h 30 -bg '#1a1a1a' -fg '#ffffff' -p

# Multi-monitor (second screen)
echo "Monitor 2" | ./src/dzen2 -xs 1 -p

# Auto-uncollapse menu on start
echo -e "Menu\nItem 1\nItem 2" | ./src/dzen2 -l 2 -e "onstart=uncollapse" -p
```

## Running E2E Tests

The project uses a screenshot-based integration test system:

```bash
# Show help and available options
./test_e2e --help

# Run all integration tests in virtual display (default)
./test_e2e TESTS.md

# Run all integration tests on native X11 display
./test_e2e --native TESTS.md

# Run a specific test by line number in virtual display
./test_e2e TESTS.md:286  # Runs "Test: 9 Block area"

# Run a specific test by line number on native X11 display
./test_e2e --native TESTS.md:286
```

### Display Modes

**Virtual Display (Default)**: `./test_e2e TESTS.md`
- Runs tests in isolated Xvfb virtual display
- Automatically starts and manages virtual X server
- Recommended for CI/CD and automated testing
- No interference with your desktop environment

**Native Display**: `./test_e2e --native TESTS.md`  
- Runs tests on your current X11 display
- Useful for debugging and visual inspection
- Tests appear on your actual screen
- Use when you need to see tests running interactively

### Running Specific Tests

You can run a specific test by appending `:<line_number>` to the test file. The line number can be any line between the test start (`## Test:`) and the next test. For example:

- `./test_e2e TESTS.md:1` - Runs "Test: 1 Color" (line 1)
- `./test_e2e TESTS.md:286` - Runs "Test: 9 Block area" (line 286)
- `./test_e2e TESTS.md:290` - Also runs "Test: 9 Block area" (line 290 is within the test)

### Testing E2E Architecture

Integration tests simulate user interactions and verify visual output:
1. `test_e2e` launches Xvfb virtual display (unless `--native` is used)
2. Runs dzen2 with test input
3. Captures screenshots with `xwd`
4. Compares against reference images using ImageMagick
5. Simulates mouse/keyboard with `xdotool`
6. Automatically cleans up virtual display on exit

The test_e2e script requires a test file parameter. Screenshots are organized by test file:
- Expected screenshots: `integration-tests/<test_basename>/expected/`
- Actual screenshots: `integration-tests/<test_basename>/actual/`
- Diff images: `integration-tests/<test_basename>/diffs/`

For example, when running `./test_e2e TESTS.md`:
- Expected: `integration-tests/TESTS/expected/05-position-padding.png`
- Actual: `integration-tests/TESTS/actual/05-position-padding.png`
- Diff: `integration-tests/TESTS/diffs/05-position-padding.png`

### Testing Xinerama Functionality

The `test_xinerama` script specifically tests multi-monitor support:

```bash
# Run Xinerama tests
make test-xinerama
# or directly
./test_xinerama
```

**Prerequisites:**
- dzen2 must be built with Xinerama support (`--enable-xinerama` configure option)
- Requires `xserver-xephyr` package for virtual monitor simulation
- Uses ImageMagick for screenshot comparison

**What test_xinerama does:**
- Uses Xephyr to create 3 virtual monitors (100x100 each in 3x1 layout)
- Tests dzen2's `-xs` option for screen-specific positioning
- Starts dzen2 on second monitor (xs=2) with red background
- Captures and compares screenshots for regression testing
- Provides colorized output (errors in red, success in green)

**Screenshot locations:**
- Expected: `integration-tests/test_xinerama/expected/test.png`
- Actual: `integration-tests/test_xinerama/actual/test.png`
- Diff: `integration-tests/test_xinerama/diffs/test.png` (on failure)

## Manual Testing with test_perfomance

The `test_perfomance` script provides a comprehensive test environment that simulates real-world dzen2 usage:

```bash
# Basic functionality test - displays just counter of update
./test_perfomance --simple

# Basic functionality test - displays complex status bar
./test_perfomance

# Memory analysis - comprehensive Valgrind check and save report to ./valgrind-out.txt
# This script runs `valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt`
timeout 10s ./test_perfomance --valgrind 2>&1; echo "Exit code: $?"

# Performance profiling - generates perf.data for analysis
./test_perfomance --perf

# Generates continuous stream of complex dzen2 markup (pipe to dzen2 if you need to run it manually)
./test_perfomance --printer
```

**What test_perfomance does:**
- Generates continuous stream of complex dzen2 markup
- Tests desktop switching, CPU/memory display, network stats, GPU info
- Includes clickable areas, colors, fonts, positioning, and graphics
- Updates every 10ms to stress-test the drawing system
- Exposes memory management issues quickly

**Analyzing test results:**
```bash
# After running Valgrind test
grep "ERROR SUMMARY" valgrind-out.txt
grep "LEAK SUMMARY" -A 5 valgrind-out.txt
grep "definitely lost" valgrind-out.txt
```

## Architecture Overview

dzen2 is a scriptable notification and menu program with two main windows:
- **Title window**: Always visible, displays single line
- **Slave window**: Optional multi-line area (enabled with `-l` option)

### Core Components

**Event Flow**: 
1. `main.c` creates windows and enters event loop
2. `action.c` handles X11 events and executes actions
3. `draw.c` renders text and graphics using the in-text formatting language
4. `caches.c` manages X11 resource caching for performance

**Key Data Structures** (in `dzen.h`):
- `Dzen`: Global state including windows, dimensions, and configuration
- `Fnt`: Font management (XFT or core fonts)
- `TWIN`: Title window state
- `SW`: Slave window with line buffer

### In-text Formatting Language

dzen2 parses special sequences in input text:
- `^fg(color)` / `^bg(color)` - Set colors
- `^fn(font)` - Change font
- `^i(icon.xbm)` - Insert XBM icon
- `^r(WxH)` / `^c(radius)` - Draw rectangle/circle
- `^p(x;y)` - Relative positioning
- `^ca(button, command)...^ca()` - Clickable areas

Parser implementation is in `draw.c:parse_line()`.

### Performance Optimizations (Fork-specific)

This fork adds caching layers:
- **Color cache**: `caches.c` caches XftColor/XColor lookups
- **Font cache**: Reuses opened fonts across operations
- **Line buffer**: Increased from 8KB to 256KB for better performance

## Development Guidelines

### Adding New Features

1. **In-text commands**: Add parsing logic to `draw.c:parse_line()`
2. **Actions**: Define in `action.h` and implement in `action.c`
3. **Configuration options**: Add to `dzen.h:Dzen` struct and parse in `main.c`
4. **Font features**: Modify `src/font.c` and `src/font.h` for font-related functionality

### Font Module

The font functionality has been extracted into a separate module consisting of:
- **`src/font.h`**: Font structure definitions and function declarations
- **`src/font.c`**: Font management implementation with XFT/non-XFT support

**Key functions:**
- `font_init()`: Initialize font system
- `font_cleanup()`: Clean up font resources
- `setfont(fontstr)`: Set current font (supports both XFT and X11 fonts)
- `textnw(font, text, len)`: Calculate text width
- `font_preload(fonts)`: Preload fonts for non-XFT builds (comma-separated list)

**Features:**
- Conditional compilation for XFT vs non-XFT builds
- Font caching for XFT builds (improves performance)
- Font preloading for non-XFT builds (allows `^fn(dfnt0)`, `^fn(dfnt1)`, etc.)
- Automatic cleanup on program exit

### Testing Changes

Always run `make test` before committing. If visual output changes are intentional:
```bash
./test_e2e TESTS.md
git add integration-tests/TESTS/expected/*.png
```

### Testing Font Module

A dedicated test script `test_font_module` verifies font functionality:

```bash
# Test current build configuration (auto-detects XFT vs non-XFT)
./test_font_module
```

**What it tests:**
- Basic font functionality (font switching with `^fn()`)
- Font and color combinations
- Font preloading (non-XFT builds only)
- Screenshot-based verification (saved to `./font_test_screenshots/`)

**Test different configurations:**
```bash
# Test without XFT (uses X11 core fonts)
make distclean
./configure --disable-xft --enable-xpm --enable-xinerama --enable-xcursor
make
./test_font_module

# Test with XFT (uses modern font rendering)
make distclean  
./configure --enable-xft --enable-xpm --enable-xinerama --enable-xcursor
make
./test_font_module
```

**Manual font testing examples:**
```bash
# XFT build - test different XFT fonts
echo "XFT: ^fn(monospace-12)Monospace^fn() ^fn(serif-14)Serif^fn() Normal" | ./src/dzen2 -p

# Non-XFT build - test X11 fonts
echo "X11: ^fn(8x16)Large^fn() ^fn(6x13)Small^fn() Normal" | ./src/dzen2 -p -fn "fixed"

# Non-XFT build - test font preloading
echo "Preloaded: ^fn(dfnt0)Font0^fn() ^fn(dfnt1)Font1^fn() Normal" | ./src/dzen2 -p -fn-preload "6x13,8x16"
```

### Common Tasks

```bash
# Run dzen2 with common options
echo "Hello World" | ./src/dzen2 -p

# Test with formatted input
echo "^fg(red)Error:^fg() Something went wrong" | ./src/dzen2 -p

# Multi-line with slave window
(echo "Title"; echo "Line 1"; echo "Line 2") | ./src/dzen2 -l 2 -p

# Test gadgets
echo "50" | ./gadgets/gdbar -w 200 -h 20

# Test Xinerama multi-monitor support
echo "Monitor 1" | ./src/dzen2 -xs 0 -p  # Display on first monitor
echo "Monitor 2" | ./src/dzen2 -xs 1 -p  # Display on second monitor
echo "Monitor 3" | ./src/dzen2 -xs 2 -p  # Display on third monitor

# Run test_perfomance with different modes
./test_perfomance # Normal test mode - displays complex dzen2 bar with live updates
timeout 10s ./test_perfomance --valgrind 2>&1; echo "Exit code: $?"  # Run with Valgrind memory checking - logs to valgrind-out.txt
./test_perfomance --perf # Run with perf profiling - creates perf.data for analysis

# Fix Valgrind file descriptor limit error (no need if run ./test_perfomance)
ulimit -n 65536  # Set before running valgrind if you get "Private file creation failed" error
```

### Debugging

1. **Configure for debugging**: Build with debug symbols and no optimization:
   ```bash
   ./configure --enable-xft --enable-xpm --enable-xinerama --enable-xcursor CFLAGS="-g -O0"
   make clean && make
   ```

2. **Using GDB**:
   ```bash
   # Basic debugging
   gdb --args ./src/dzen2 -p
   
   # With X11 synchronous mode (helps with X11 event debugging)
   gdb --args ./src/dzen2 -sync -p
   ```

3. **Common GDB workflow**:
   - `break main` - Set breakpoint at main function
   - `run` - Start execution
   - `continue` - Continue after breakpoint
   - `bt` - Show backtrace

4. **Memory Debugging with Valgrind**:
   Instructions:

   ```bash
   # Run for specific duration and analyze
   timeout 10s ./test_perfomance --valgrind 2>&1; echo "Exit code: $?" # Stop after 10 seconds
   cat valgrind-out.txt | grep "ERROR SUMMARY"      # Check error count
   cat valgrind-out.txt | grep "LEAK SUMMARY" -A 5  # Check memory leaks
   
   # Run valgrind manually 10seconds:
   ./test_perfomance --printer | valgrind --leak-check=full --track-origins=yes ./src/dzen2 -p 10; echo "Exit code: $?"
   ```

## Code Style

- Keep line length under 120 characters
- Use `make format` after finishing modifying all files or before committing
- Ensure new files are part of distribution, file must be included in Makefile.am (for example EXTRA_DIST)

