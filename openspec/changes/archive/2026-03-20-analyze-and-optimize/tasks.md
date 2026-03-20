## 1. Bug Fixes

- [x] 1.1 Fix typo: rename `_prebuffer_tanslated` to `_prebuffer_translated` everywhere in `src/main.cpp`
- [x] 1.2 Fix `find_first_new_line`: replace substring search with line-set comparison — split `prebuffer` into lines, build an `unordered_set<string>`, check each candidate line for exact membership
- [x] 1.3 Surface errors in `get_livecaptions()` catch blocks: log `e.message()` / `e.what()` to `std::cerr` instead of silently swallowing exceptions

## 2. Eliminate Duplicate UIAutomation Calls

- [x] 2.1 Add a `ParsedCaptions` struct (or use the existing `pair<string, vector<string>>`) as a shared result type
- [x] 2.2 Refactor `output_captions()` and `output_translate()` to accept a pre-parsed `ParsedCaptions` argument instead of calling `parse_captions()` internally
- [x] 2.3 Update the ASIO coroutine in `main()` to call `parse_captions()` once per 10-second tick — but only when at least one consumer is due (`need_translate || need_flush`); pass the result to both methods

## 3. Fix Translation Output Sink

- [x] 3.1 Change `ostream_translate_line` and `ostream_translate` default stream parameter from `std::cerr` to accept the stream determined by the output mode
- [x] 3.2 Update `output_translate()` to pass `std::cout` (when output is `-`) or `*_output_file` (when writing to a file) as the target stream
- [x] 3.3 Acquire `_file_mutex` in `output_translate()` before writing to `*_output_file` (mirror the pattern in `output_captions()`)

## 4. CURL Timeout

- [x] 4.0 Set a 10-second CURL connect+operation timeout in `LibreTranslateAPI` (or at the call site in `ostream_translate_line`) via `curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L)` to bound the worst-case ASIO loop stall

## 5. Configurable Intervals

- [x] 5.1 Add `--poll-interval <seconds>` CLI argument to `argparse` with default value 10 and validation ≥ 1
- [x] 5.2 Add `--flush-interval <seconds>` CLI argument to `argparse` with default value 60 and validation ≥ 1
- [x] 5.3 Thread the parsed interval values through to the ASIO coroutine timers (`timer_10s` / `timer_60s` expiry calls)
- [x] 5.4 Update the startup message (`"Save content into file, every 1 min."`) to reflect the actual configured flush interval

## 6. Tests

- [x] 6.1 Create `tests/test_find_first_new_line.cpp` with `assert()`-based cases covering: exact match, false-match (short line is substring of buffered line), all-seen, all-new, empty prebuffer
