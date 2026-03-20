## Context

All logic lives in `src/main.cpp` (~350 lines). The `Engine` class handles UIAutomation access, caption diffing, file I/O, and optional translation. `main()` drives two timers (10s poll, 60s flush) via a single ASIO coroutine. There are no tests.

Key current state problems:
- Per cycle, when both `--translate` and file output are due, `parse_captions()` calls `get_livecaptions()` twice (two UIAutomation round-trips)
- `find_first_new_line` uses `prebuffer.find(lines[i])` — substring search that can match a short line inside a longer buffered line, causing skipped output
- All exceptions in `get_livecaptions()` are silently swallowed
- Translation output hardcoded to `std::cerr`; file-mode users never see translated text in their output file
- `_prebuffer_tanslated` typo persists in the codebase
- 10s/60s intervals are compile-time constants with no way to override

## Goals / Non-Goals

**Goals:**
- Eliminate the double UIAutomation call per cycle
- Fix the `find_first_new_line` substring false-match bug
- Surface errors from silent catch blocks to `std::cerr`
- Route translated output to the same sink as captions (file or stdout)
- Fix `_prebuffer_tanslated` typo
- Add `--poll-interval` and `--flush-interval` CLI args (seconds, integers)

**Non-Goals:**
- Adding a target-language flag for translation (separate concern)
- Abstracting Engine into multiple classes or files
- Async translation (currently synchronous CURL call blocks the ASIO thread)

## Decisions

### D1: Single `parse_captions()` result shared between translation and caption output in the same tick

**Decision**: Cache the `parse_captions()` result at the top of the 10-second coroutine body and pass it as an argument to `output_translate()` and `output_captions()` instead of each calling `parse_captions()` internally.

**Why**: Avoids two UIAutomation round-trips per cycle. UIAutomation COM calls have non-trivial overhead and can fail intermittently; reducing call frequency improves reliability.

**Alternative considered**: Keep internal calls but add a TTL-based cache on `get_livecaptions()` result. Rejected — adds statefulness and a time dependency; the single-call approach is simpler and covers the 10s window entirely.

### D2: Fix `find_first_new_line` with whole-line matching

**Decision**: Compare each candidate line against the prebuffer using line-boundary-aware matching (split prebuffer into lines and use set/vector lookup) rather than raw substring search.

**Why**: `prebuffer.find("is")` matches inside `"This is a test"` — a short new caption line that happens to appear as a substring of any previously buffered line will be silently skipped.

**Alternative considered**: Use `\n<line>\n` sentinel wrapping. Rejected — edge cases at buffer start/end require special handling; splitting both sides into a set is simpler and O(n).

### D3: Translation output follows the configured sink

**Decision**: `ostream_translate` / `ostream_translate_line` default parameter changes from `std::cerr` to the same stream used by `ostream_captions`. `output_translate()` passes `std::cout` (stdout mode) or `*_output_file` (file mode).

**Why**: Users who write to a file expect translated text alongside captions. Stdout mode (`-o -`) already mixes both on stdout which is the natural expectation.

**Alternative considered**: Add a separate `--translate-output` flag. Rejected — unnecessary complexity; a single output sink per invocation is the existing model.

### D4: Configurable intervals as plain integer CLI args

**Decision**: `--poll-interval <seconds>` (default 10) and `--flush-interval <seconds>` (default 60). Stored as `int`, validated ≥ 1.

**Why**: Power users capturing fast-moving captions may want sub-10s polling; others writing large files may want longer flush intervals. Integer seconds is sufficient granularity.

### D5: Mutex protection in output_translate() when writing to file

**Decision**: Wrap the `*_output_file` write path in `output_translate()` with `scoped_lock(_file_mutex)`, mirroring `output_captions()`.

**Why**: Consistency with existing pattern. `shutdown()` acquires `_file_mutex` before flushing and closing the file. Having `output_translate()` write to the same file without the lock is a maintenance trap — safe today (single ASIO thread), but silently broken if a background thread is ever added.

**Alternative considered**: Leave unlocked with a comment. Rejected — comments are not enforced; the lock costs nothing at runtime.

### D6: 10-second CURL operation timeout

**Decision**: Set `CURLOPT_TIMEOUT` to 10 seconds on the CURL handle used for LibreTranslate calls.

**Why**: The ASIO loop runs on a single thread. A synchronous CURL call with no timeout can block for up to 300 seconds (system default) if LibreTranslate is unreachable — during which time the 10s poll timer drifts, the window-exit check is skipped, and Ctrl+C is ignored. A 10s timeout bounds the worst case to match the poll interval.

**Alternative considered**: Make translation async (separate thread / ASIO strand). Deferred — significantly more complex. The timeout is the minimal, "boring" fix.

## Risks / Trade-offs

- [Behavioral change] Translation now goes to the output file/stdout instead of stderr. Users who parse stderr for translated text will break. → Documented in CHANGELOG; old behavior was arguably a bug.
- [Risk] Sharing a single `parse_captions()` result requires the result type to be passed through the call stack. The change is contained to `main.cpp` private methods, so impact is low.
- [Risk] `find_first_new_line` fix changes output cadence — previously skipped lines may now appear. → Correct behavior; regression only for users relying on the broken deduplication.
