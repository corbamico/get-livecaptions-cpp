# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Windows-only C++ utility that captures real-time content from Windows Live Captions (Win+Ctrl+L) via UIAutomation and optionally translates it using LibreTranslate. Uses C++/WinRT, ASIO, and UIAutomation APIs.

## Build Commands

Requires Visual Studio 2022 or 2026, CMake 3.25+, and vcpkg (git submodule). The preset uses `"Visual Studio 18 2026"` — update `CMakePresets.json` if using VS 2022 (`"Visual Studio 17 2022"`).

```bash
# Configure (run once)
cmake --preset vs2026-x64

# Build Release
cmake --build --preset release

# Build Debug
cmake --build --preset debug

# Build with statically linked zlib/libcurl
cmake --build --preset release-static
```

Build output goes to `builds/vs2026-x64/` or `builds/ninja-multi-vcpkg/`.

There are no automated tests.

## Architecture

The entire application logic lives in `src/main.cpp`. The `Engine` class wraps all UIAutomation access:

- **`get_livecaptions()`** — reads text from the `CaptionsTextBlock` automation element inside the `LiveCaptionsDesktopWindow` window class
- **`parse_captions()` / `find_first_new_line()`** — detects newly appeared caption lines since the last poll
- **`output_captions()` / `output_translate()`** — writes captions to file or stdout, optionally calling LibreTranslate

`main()` sets up two ASIO coroutine timers:
- **10-second loop**: polls Live Captions and optionally translates
- **60-second loop**: flushes accumulated captions to the output file

Concurrency: mutex (`_file_mutex`) protects file I/O; `_shutdown_requested` atomic flag coordinates graceful shutdown on SIGINT/SIGTERM.

## Dependencies

Managed via vcpkg (submodule) and CPM:
- **asio** — async event loop and timers
- **wil** — COM smart pointer helpers
- **argparse** — CLI argument parsing
- **curl** — HTTP client for LibreTranslate API
- **LibreTranslate-cpp** — fetched at configure time via CPM from GitHub

Windows-specific: requires `windowsapp`, `RuntimeObject.lib`, UIAutomation, and C++/WinRT.

## Key Runtime Behavior

- Polls every 10 seconds; writes to file every 60 seconds
- Output file `-` means stdout
- `--translate <lang>` sends captions to LibreTranslate at `--translate-host` (default `http://127.0.0.1:5000`)
- UIAutomation element (`CaptionsTextBlock`) is cached after first lookup
- COM initialized as single-threaded apartment (STA) in the Engine constructor
