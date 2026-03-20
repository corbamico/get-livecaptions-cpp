# TODOS

## Async Translation

**What:** Make the LibreTranslate HTTP call in `output_translate()` asynchronous so it does not block the ASIO event loop thread.

**Why:** Currently every translation tick blocks the single ASIO thread for the round-trip duration. The 10s CURL timeout bounds the worst case, but every tick still introduces latency — the 10s poll timer drifts by the actual LibreTranslate response time.

**Pros:** Timer loop stays fully responsive during translation; Ctrl+C processes immediately; window-exit check is never delayed by a slow LibreTranslate server.

**Cons:** Adds complexity (thread pool or detached thread + result posting back to ASIO context); needs care around `_output_file` lifetime and mutex when posting results across threads.

**Context:** This was explicitly listed as a Non-Goal in the `analyze-and-optimize` design doc (D6 decision). The CURL timeout mitigation was chosen as the minimal "boring" fix. Async translation is the correct long-term direction for a real-time caption tool. Starting point: wrap the CURL call in `asio::post(thread_pool, ...)` and post the translated result back with `asio::post(io_context, ...)`.

**Depends on:** `analyze-and-optimize` change (adds the CURL timeout baseline and the single-call parse_captions() refactor).
