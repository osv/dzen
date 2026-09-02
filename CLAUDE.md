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

# format (clang) all C files. Run after .h .c changes
make format
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
└── tests/
    ├── unit/         # Automake-managed C unit tests
    ├── helpers/      # Helper executables for integration tests
    ├── integration/  # Signal, visual, Xinerama, and XRandR suites
    └── tools/        # Manual performance and profiling tools
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

## Testing

Tests are grouped by purpose:

```text
tests/unit/                  C unit tests managed by Automake
tests/helpers/               helper executables used by integration tests
tests/integration/signals/   signal and process lifecycle tests
tests/integration/visual/    Markdown-driven XFT and core-font visual suites
tests/integration/xinerama/  isolated Xvfb + Xephyr multi-screen test
tests/integration/xrandr/    isolated Xorg dummy-output tests
tests/tools/                 manual performance and profiling tools
```

Run every test applicable to the current build with `make check` or its `make
test` alias. Useful focused targets are `make test-unit`, `make test-signals`,
`make test-visual`, `make test-xinerama`, and `make test-xrandr`. Targets that
need a disabled feature, such as `make test-nonxft`, fail with a configure hint
when used with an incompatible build.

The visual runner starts an isolated Xvfb by default:

```bash
tests/integration/visual/runner.sh tests/integration/visual/xft/cases.md
tests/integration/visual/runner.sh tests/integration/visual/xft/cases.md:286
tests/integration/visual/runner.sh --native tests/integration/visual/xft/cases.md
```

A line suffix selects the test block containing that line. Expected images,
actual captures, and diffs live beside each suite in `expected/`, `actual/`, and
`diffs/`. Never create or replace an expected image implicitly. Review actual
images first, then update the relevant suite explicitly:

```bash
UPDATE_VISUAL_EXPECTED=1 make test-visual
UPDATE_XINERAMA_EXPECTED=1 make test-xinerama
UPDATE_XRANDR_EXPECTED=1 make test-xrandr
```

After reviewing an intentional change, add only files under the suite's
`expected/` directory. The `actual/` and `diffs/` directories are ignored.

Integration output uses green for PASS, red for FAIL/error, yellow for
SKIP/warnings, and gray for TEST/INFO headings. Colors are enabled on a terminal
and in GitHub Actions. Set `NO_COLOR=1` to disable them.

Feature requirements:

- XFT visual cases require `--enable-xft`; core-font cases require `--disable-xft`.
- Xinerama requires `--enable-xinerama`, Xvfb, Xephyr, xset, and ImageMagick.
- XRandR requires `--enable-xrandr --enable-xft`, the Xorg dummy driver,
  xrandr/xdotool utilities, ImageMagick, fontconfig, and DejaVu Sans Mono Book.

The manual performance tool is not part of `make check`:

```bash
timeout 10s tests/tools/performance_benchmark.sh
timeout 10s tests/tools/performance_benchmark.sh --valgrind
timeout 10s tests/tools/performance_benchmark.sh --perf
tests/tools/performance_benchmark.sh --bench
tests/tools/performance_benchmark.sh --printer 1000
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
5. **Documentation**: Every major user-visible feature must be added to the
   "About this dzen2 fork" differences list in `README.dzen`. Regenerate
   `README.md` and `dzen2.1` after changing `README.dzen`.

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

Always run `make check` before committing. If visual output changes are intentional:
```bash
UPDATE_VISUAL_EXPECTED=1 make test-visual
# Review the PNG files before adding them.
git add tests/integration/visual/*/expected/*.png
```

For C tests under `tests/`, include `test_common.h` and use `CHECK(...)` for
validation. Do not use the standard `assert()` macro or place side effects in
it: assertions disappear when compiled with `-DNDEBUG`, which can turn a test
into a false pass or skip the operation being tested. `CHECK(...)` remains
active in all build modes and reports the failing source file and line.

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

# Run performance_benchmark.sh with different modes
tests/tools/performance_benchmark.sh # Normal test mode - displays complex dzen2 bar with live updates
timeout 10s tests/tools/performance_benchmark.sh --valgrind 2>&1; echo "Exit code: $?"  # Run with Valgrind memory checking - logs to valgrind-out.txt
tests/tools/performance_benchmark.sh --perf # Run with perf profiling - creates perf.data for analysis

# Fix Valgrind file descriptor limit error (no need if run tests/tools/performance_benchmark.sh)
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
   timeout 10s tests/tools/performance_benchmark.sh --valgrind 2>&1; echo "Exit code: $?" # Stop after 10 seconds
   cat valgrind-out.txt | grep "ERROR SUMMARY"      # Check error count
   cat valgrind-out.txt | grep "LEAK SUMMARY" -A 5  # Check memory leaks
   
   # Run valgrind manually 10seconds:
   tests/tools/performance_benchmark.sh --printer | valgrind --leak-check=full --track-origins=yes ./src/dzen2 -p 10; echo "Exit code: $?"
   ```

## Code Style

- Keep line length under 120 characters
- Use `make format` after finishing modifying all files or before committing
- Ensure new files are part of distribution, file must be included in Makefile.am (for example EXTRA_DIST)
