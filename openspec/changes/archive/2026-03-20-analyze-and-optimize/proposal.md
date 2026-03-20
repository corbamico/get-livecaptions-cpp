## Why

The codebase has several correctness bugs, silent error suppression, and inefficiencies (duplicate UIAutomation calls per cycle) that degrade reliability and debuggability. Addressing these makes the tool more predictable and easier to maintain.

## What Changes

- Fix typo: `_prebuffer_tanslated` → `_prebuffer_translated` throughout
- Fix `find_first_new_line` logic: current substring search can produce false matches when a short line appears inside a different longer buffered line
- Eliminate duplicate `parse_captions()` / UIAutomation calls per 10-second cycle when both `--translate` and file output are active in the same tick
- Route translated output to the configured output stream (file or stdout) instead of hardcoded `std::cerr`
- Surface errors from silent catch blocks (`catch (winrt::hresult_error &e){}`) — log to `std::cerr` so failures are visible
- Make polling intervals (`--poll-interval`, `--flush-interval`) configurable via CLI arguments with current defaults (10s / 60s)
- Add 10-second CURL timeout to bound ASIO event loop stall when LibreTranslate is unreachable
- Protect `output_translate()` file writes with `_file_mutex` (consistency with `output_captions()`)

## Capabilities

### New Capabilities
- `configurable-intervals`: CLI flags `--poll-interval` and `--flush-interval` to override the hardcoded 10s/60s timers
- `translation-output`: Translation output routed to the configured output sink (file or stdout) instead of stderr

### Modified Capabilities
- None (no existing specs directory)

## Impact

- `src/main.cpp` — all changes are contained here
- No new dependencies required
- CLI interface gains two optional arguments (backwards-compatible)
- Translation output destination changes from `std::cerr` to the configured output sink — visible behavioral change for users relying on stderr for translated text
