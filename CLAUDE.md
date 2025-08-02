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

## Running E2E Tests

The project uses a screenshot-based integration test system:

```bash
# Run all integration tests
./test_e2e
```

Tests are defined in `TESTS.md` and compare screenshots against reference images in `integration-tests/`.
Files `reference_*.png` are reference screenshots automatically created for TESTS.md and should be committed after adding new tests.
For example, if reference screenshot name is `./integration-tests/reference_05-position-padding.png`
Than actual screenshot will be stored without `reference_` prefix as `integration-tests/05-position-padding.png`.
Diff between actual and reference stored without prefix `reference_` but with prefix `diff_` - `integration-tests/diffs/diff_05-position-padding.png`.

### Testing E2E Architecture

Integration tests simulate user interactions and verify visual output:
1. `test_e2e` launches Xvfb virtual display
2. Runs dzen2 with test input
3. Captures screenshots with `xwd`
4. Compares against reference images using ImageMagick
5. Simulates mouse/keyboard with `xdotool`

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
./test_e2e
git add integration-tests/reference_*.png
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
   
   # Run valgrind manually:
   timeout 10s ./test_perfomance --printer | valgrind --leak-check=full --track-origins=yes ./src/dzen2 -p; echo "Exit code: $?"
   ```

## Code Style

- Keep line length under 120 characters
- Use `make format` after finishing modifying all files or before committing
