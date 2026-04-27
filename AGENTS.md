# BiliTicketMonitor Architecture Knowledge Base

> This document serves as the single source of truth for all agents working on the BiliTicketMonitor project. It documents both the C++ core (`src/main.cpp`) and the Python stock monitor (`extra/HSR_Land_Stock_Monitor.py`), with a detailed comparison and merge strategy.

---

## 1. Project Overview

**BiliTicketMonitor** is a Bilibili ticketing event monitor that polls ticket availability and notifies when stock status changes. It consists of two independent implementations in the same repository:

| Component | Language | Lines | File |
|-----------|----------|-------|------|
| Core Monitor | C++20 | 852 | `src/main.cpp` |
| Stock Monitor | Python 3 | 313 | `extra/HSR_Land_Stock_Monitor.py` |

### Build System
- CMake-based build via `CMakeLists.txt`
- Dependencies: `libcurl` (HTTP), `cJSON` (JSON parsing, header-only, bundled in `src/`)
- Output: `build/BiliTicketMonitor` (Linux)

### Runtime Dependencies
- C++ side: libcurl, libcJSON (embedded header), C++20 standard library
- Python side: `requests`, `urllib3`, `colorama` (installed via `pip install -r requirements.txt`)

### Author
- FriendshipEnder (https://space.bilibili.com/180327370)
- Project URL: https://github.com/fsender/BiliTicketMonitor

---

## 2. main.cpp Deep Analysis

### Architecture

The C++ monitor (`src/main.cpp`, v2.0.0) is a single-file, single-threaded application structured around three core types:

#### Config Class (lines 88-321)
- Static class managing all configuration via `static` members
- `config.txt` format: 7 lines (ticket_id, ticket_no, bat_path, refresh_interval_ms, timeout_ms, api_base_url, user_agent_header) + comment section
- Supports CLI arguments: `--id`, `--ticket-no`, `--interval`, `--script` (paired-key format: `--id 102194 --ticket-no 1 --interval 300 --script bhyg.bat`)
- `checkconf()` reads and validates `config.txt`. On failure, deletes old config and prompts interactive input
- Default ticket ID: `102194` (BW2025)

#### HttpResponse Struct (lines 323-328)
```cpp
struct HttpResponse {
    string data;      // Response body (raw JSON)
    long status_code; // HTTP status code
    string error;     // libcurl error string on failure
};
```

#### Monitor Class (lines 547-619)
- Single-threaded polling loop: `run_monitor()` continuously calls `http_get()` then `process_data()`
- Uses `last_data` (vector<vector<string>>) for diff detection to avoid redundant table renders
- `selling` flag set when the monitored ticket's status becomes "预售中" (on sale)
- On `selling == true`, executes the batch script via `system(Config::BATPATH.c_str())`
- Error handling: 412 triggers critical stop; other errors set `healthy = false` temporarily
- `show_table()` renders a formatted TUI with ANSI color codes for status display

#### HTTP Layer (lines 357-387)
- `http_get()` wraps libcurl for synchronous GET requests
- `WriteCallback` is a C-style curl write function that appends to a `std::string`
- Uses `curl_slist` for custom headers; SSL verification disabled
- Timeout configurable via `Config::TIMEOUT` (default 10000ms)

#### JSON Parsing (lines 390-461)
- `process_data()` uses cJSON to parse the project/getV2 response
- Extracts `data.name`, `data.screen_list[*].ticket_list[*]`
- Reads `screen_name`, `desc`, `sale_flag.display_name` for each ticket
- Returns `(project_name, vector<vector<string>>)` where each inner vector is `{ticket_name, status}`

#### Status Color Mapping (lines 536-543)
```
已售罄 -> Red
已停售 -> Red
不可售 -> Red
未开售 -> Cyan
暂时售罄 -> Yellow
预售中 -> Green
```

#### Key Design Decisions
- **Single API only**: Uses GET `project/getV2` which returns project-level info with text-based status detection
- **Text matching**: Relies on `sale_flag.display_name` string values like "预售中" to determine availability
- **Batch script trigger**: When target ticket goes "预售中", runs an external `.bat`/`.sh`/`.ps1` file
- **No concurrency**: Everything runs on the main thread with `sleep_for()` between polls
- **Terminal-centric**: Heavy use of ANSI escape codes for colored TUI output

---

## 3. HSR_Land_Stock_Monitor.py Deep Analysis

### Architecture

The Python monitor (`extra/HSR_Land_Stock_Monitor.py`) is a multi-target, concurrent ticket stock checker with push notifications.

#### Config Class (lines 15-42)
- All configuration via static class attributes
- `TARGETS`: List of dicts with `screen_id`, `sku_id`, `label` for multi-target monitoring
- `BARK_ENABLED`, `BARK_SERVER`, `BARK_KEY`: Bark push notification settings
- `STOCK_CHECK_URL`: POST `https://show.bilibili.com/api/ticket/stock/check`

#### Monitor Class (lines 53-264)
- Two daemon threads: `show_time()` for status bar, `run_monitor()` for stock polling
- `self.stop` is a `threading.Event` for clean shutdown
- `self.print_lock` is a `threading.Lock` for thread-safe console output
- `self.last_stock_status` tracks per-target stock state for diff detection
- `self.request_count` accumulates total API calls for status bar display

#### HTTP Layer
- Uses `requests.Session` with SSL verification disabled
- Session mounts `HTTPAdapter` with connection pool sized to `max_workers`
- POST body: `{"projectId": ..., "skuId": ..., "screenId": ...}`

#### Stock Check (lines 156-188)
- `check_stock()` sends POST to stock/check API
- Returns `stockStatus` integer: 1=暂时售罄, 2=已售罄, 3=有库存, -1=error
- Parses `data.stockStatus` from JSON response
- Distinguishes between ValueError (bad JSON) and RequestException (network error)

#### Bark Push (lines 73-106)
- `send_bark()` sends POST to `{BARK_SERVER}/{BARK_KEY}` with JSON payload
- Payload includes: `title`, `body`, `group`, `level` ("critical" for stock, "active" otherwise)
- Optional: `sound` (for stock alerts), `icon`
- Runs in daemon thread to not block main monitor loop

#### Concurrency (lines 194-245)
- `ThreadPoolExecutor(max_workers=len(TARGETS))` runs stock checks in parallel
- Each target's `screen_id` + `sku_id` is a separate request
- Results collected via `future.result()`, ordered by submission
- Status changes trigger Bark push + console print with millisecond timestamp

#### Key Design Decisions
- **Per-SKU precision**: Uses `stock/check` POST API with specific `skuId` for exact stock detection
- **Integer status codes**: `stockStatus` is a reliable integer, not text matching
- **Multi-target parallel**: Each target gets its own thread via ThreadPoolExecutor
- **Bark push**: Mobile notification via Bark iOS app webhook
- **Session reuse**: Single `requests.Session` with connection pooling for efficiency

---

## 4. Feature Comparison Matrix

| Feature | main.cpp (C++) | HSR_Land_Stock_Monitor.py (Python) |
|---------|----------------|-----------------------------------|
| **API Endpoint** | GET `project/getV2` | POST `stock/check` |
| **Detection Granularity** | Project-level (all tickets) | Per-SKU (screen_id + sku_id) |
| **Stock Detection** | Text match on `sale_flag.display_name` | Integer `stockStatus` code |
| **Concurrency** | Single-threaded | ThreadPoolExecutor (parallel) |
| **Multi-Target** | Single ticket_no | Multiple screen_id+sku_id pairs |
| **Polling Interval** | Configurable (REFRESH_INTERVAL ms) | Implicit (loop + future.result()) |
| **Notification** | Batch script execution (bat/sh/ps1) | Bark iOS push notification |
| **Error Recovery** | Sets healthy=false, continues | Per-future exception handling |
| **Config Persistence** | config.txt (7-line format) | Hardcoded Config class + CMD input |
| **CLI Arguments** | --id, --ticket-no, --interval, --script | None (all in Config class) |
| **Session Reuse** | New curl handle per request | requests.Session with connection pool |
| **SSL Verification** | Disabled (libcurl) | Disabled (requests + urllib3) |
| **Thread Safety** | N/A (single-threaded) | threading.Lock for print output |
| **Status Bar** | Green timestamp line | Time + request count in status bar |
| **External Deps** | libcurl, cJSON (bundled) | requests, urllib3, colorama |
| **Platform** | Linux/Windows (system() calls) | Cross-platform (pure Python) |
| **Auto-Test Bark** | N/A | Yes (on startup) |
| **Config Validation** | Full (7-line format + type checks) | None (trust Config class) |

---

## 5. API Comparison Analysis

### GET project/getV2 (main.cpp)
- **URL**: `https://show.bilibili.com/api/ticket/project/getV2?version=134&id={ticket_id}`
- **Method**: GET
- **Response structure**: Nested JSON with `data.screen_list[*].ticket_list[*]`
- **Stock info**: Embedded as `sale_flag.display_name` text strings
- **Strengths**: Returns full project info (name, all screens, all tickets) in one call
- **Weaknesses**: Text-based status is fragile; no explicit stock count; returns all tickets even if only one matters

### POST stock/check (Python)
- **URL**: `https://show.bilibili.com/api/ticket/stock/check`
- **Method**: POST with JSON body `{"projectId": ..., "skuId": ..., "screenId": ...}`
- **Response structure**: `{"data": {"stockStatus": 1|2|3}}`
- **Stock info**: Integer status code (1=暂时售罄, 2=已售罄, 3=有库存)
- **Strengths**: Precise per-SKU check; integer status is reliable; lightweight response
- **Weaknesses**: One call per target (requires concurrency for multi-target); doesn't return ticket names/descriptions

### Recommended Dual-API Strategy for Merged Code
The merge should use **both APIs** complementarily:
1. **Startup phase**: Call `project/getV2` once to fetch project name, screen list, and ticket descriptions
2. **Polling phase**: Call `stock/check` for each configured target (screen_id + sku_id) with the integer stockStatus
3. **Benefits**: Best of both worlds — rich initial info + precise per-SKU polling

---

## 6. Merge Strategy

### Goal
Extend the C++ monitor with Python's features (multi-target, stock/check API, Bark push, concurrency) while maintaining backward compatibility with `config.txt` and existing CLI arguments.

### Constraints
- **No new external dependencies**: Only libcurl + cJSON (already in use)
- **config.txt backward compatibility**: Existing 7-line format must still work
- **C++20 only**: Use modern C++ features (std::format, std::jthread, etc.)

### Design Blueprint

#### New Types to Add

**BarkClient class** (new)
- `sendBark(title, body, isStock)` — POST to Bark webhook URL
- Config fields: `BARK_ENABLED`, `BARK_SERVER`, `BARK_KEY`, `BARK_GROUP`, `BARK_SOUND`, `BARK_ICON`
- Runs in daemon thread (std::jthread or separate std::thread)

**StockTarget struct** (new)
- `screen_id` (int), `sku_id` (int), `label` (string)
- Replaces single `TICKET_ID` + `TICKETNO` with multi-target list

**HttpResponse extension** (modify)
- Already exists; add `stockStatus` field for POST response parsing

**process_stock_data()** (new function)
- Parse stock/check JSON response: extract `data.stockStatus`
- Returns integer (1, 2, 3, or -1 on error)

**http_post()** (new function)
- Mirror of `http_get()` but for POST requests
- Accepts JSON body string and content-type header
- Returns `HttpResponse` with same struct

#### Config Class Changes

Extend to support new fields:
```
Line 1: ticket_id (unchanged)
Line 2: ticket_no (unchanged, for backward compat)
Line 3: bat_path (unchanged)
Line 4: refresh_interval_ms (unchanged)
Line 5: timeout_ms (unchanged)
Line 6: api_base_url (unchanged)
Line 7: user_agent_header (unchanged)
Line 8: bark_enabled (0/1) — NEW
Line 9: bark_server (string) — NEW
Line 10: bark_key (string) — NEW
Line 11: bark_sound (string) — NEW
Line 12: bark_group (string) — NEW
```
- Lines 8+ are optional; if fewer than 8 lines, default to disabled Bark

Add multi-target config:
- Parse `targets` from a separate format or new config section
- Each target: `screen_id,sku_id,label`

#### Monitor Class Changes

**run_monitor()** becomes multi-threaded:
1. Launch `show_time()` thread (same as current behavior)
2. Launch `stock_check_loop()` thread with ThreadPoolExecutor equivalent
3. Use `std::jthread` (C++20) for automatic join on destruction
4. Each target gets its own curl session (or reuse a shared session with per-thread curl handles)

**Thread-safe print**: Add `std::mutex` + `std::condition_variable` for console output

**Dual-API flow**:
1. On startup: call `project/getV2` once to get project name and ticket descriptions
2. On each poll: call `stock/check` for each target concurrently
3. On stock status change: print diff + trigger Bark push (if enabled)

**Backward compat path**:
- If `TARGETS` is empty and only `TICKET_NO` is set (old config format), fall back to `project/getV2` polling mode (current behavior)
- This preserves existing user experience

#### ThreadPool Implementation

C++20 `std::jthread` + `std::async` or a simple thread pool:
- Pool of `min(num_targets, hardware_concurrency)` worker threads
- Each worker: curl_easy_init(), POST to stock/check, curl_easy_cleanup()
- Results collected via `std::promise`/`std::future` or thread-safe queue

#### Bark Implementation

```cpp
class BarkClient {
    std::string server;
    std::string key;
    std::string sound;
    std::string group;
    bool enabled;
    std::thread push_thread;
    
    void pushLoop();           // Polls a queue for push requests
    void enqueue(const string& title, const string& body, bool isStock);
    static size_t writeCallback(void*, size_t, size_t, void*);
    bool send(const string& title, const string& body, bool isStock);
};
```

---

## 7. Merge TODOs

### Phase 1: Foundation
- [ ] Add `http_post()` function mirroring `http_get()` for POST requests
- [ ] Add `process_stock_data()` to parse stock/check JSON response
- [ ] Add `StockTarget` struct definition
- [ ] Extend `Config` class with Bark-related static members
- [ ] Extend `config.txt` parsing to handle new optional lines 8+

### Phase 2: Multi-Target
- [ ] Add multi-target parsing to Config (from config.txt or new format)
- [ ] Implement thread-safe console output (`std::mutex` for print sections)
- [ ] Add `show_time()` daemon thread (currently inline in run_monitor)

### Phase 3: Concurrency
- [ ] Implement simple thread pool or use `std::async` for parallel stock checks
- [ ] Modify `run_monitor()` to launch concurrent stock/check per target
- [ ] Add stock status diff detection (replace current ticket diff)
- [ ] Add per-target status tracking map (like `last_stock_status`)

### Phase 4: Bark Push
- [ ] Implement `BarkClient` class with curl POST to Bark webhook
- [ ] Add Bark push queue (thread-safe) and background push thread
- [ ] On stock status == 3, enqueue Bark push with title/body
- [ ] On stock status == 1, enqueue Bark push with "暂时售罄" alert
- [ ] Add startup Bark key input (interactive, like Python version)

### Phase 5: Dual-API Integration
- [ ] On startup, call project/getV2 once for project name and ticket descriptions
- [ ] Map screen_id + sku_id from stock/check results to ticket descriptions
- [ ] Display rich ticket info (name, desc) alongside stock status
- [ ] Preserve old single-ticket text-matching fallback for backward compat

### Phase 6: Testing & Polish
- [ ] Build test: `cmake --build build` compiles cleanly
- [ ] Config backward compat: existing 7-line config.txt works unchanged
- [ ] Test with real Bilibili ticket APIs (or mock)
- [ ] Test multi-target concurrent polling
- [ ] Test Bark push with sample key
- [ ] Test 412 rate-limit recovery
- [ ] Add `--targets` CLI argument for multi-target mode
- [ ] Update README with new features

---

## 8. Risks & Gotchas

### API-Specific Risks
- **412 Rate Limit**: Both APIs can return 412 if polled too fast. Python version handles it by stopping. C++ version also stops on 412. Consider exponential backoff instead of hard stop.
- **stock/check requires valid skuId**: Invalid sku_id returns `stockStatus: -1` (or error). Must validate targets before polling.
- **project/getV2 response format may change**: The nested screen_list/ticket_list structure has changed before. The merged code should handle missing fields gracefully.

### Concurrency Risks
- **Curl thread safety**: `curl_easy_init()` must be called per-thread (not shared). Each worker thread needs its own curl handle.
- **Memory in callbacks**: The WriteCallback receives `void*` which must be cast to `std::string*` safely. Ensure string is alive during callback.
- **Console output race**: Multiple threads printing simultaneously causes garbled output. Must use mutex around all `cout` operations.

### Config Risks
- **Line 3 (BATPATH)**: Can be empty string (no script). Must not break on empty path. Current code handles this with `if (Config::BATPATH == "")`.
- **config.txt encoding**: Must handle Windows CRLF vs Unix LF line endings. `getline()` handles both.
- **Extending config.txt**: Adding lines 8+ breaks old configs. Must be backward compatible: if file has < 8 lines, use defaults for new fields.

### Performance Risks
- **curl overhead**: Creating/destroying curl handles per-request is expensive for high-frequency polling. Consider connection reuse per-thread.
- **Thread count**: With N targets, N concurrent curl requests per poll cycle. For large N, this could overwhelm the IP rate limit.
- **Memory**: cJSON creates new objects per parse. Must call `cJSON_Delete()` on every response (current code does this correctly).

### Platform Risks
- **system() call**: `system(Config::BATPATH.c_str())` blocks until script exits. Consider `std::async` for non-blocking execution.
- **Windows compatibility**: `chcp 65001` for UTF-8, `cls` for clear. The merged code should check `#ifdef _WIN32` for platform-specific behavior.
- **ESP32 mention**: Comment at line 10 says the project may run on Arduino-ESP32. libcurl is available there, but `std::format` may not be. Consider `#ifdef` for ESP32 compatibility.

### Behavioral Risks
- **Old vs new mode**: When should the merged code use stock/check vs project/getV2? Decision: if multi-target is configured, use dual-API. If single ticket_no (old mode), use project/getV2 only for backward compat.
- **selling flag logic**: Current code sets `selling = (row[1] == "预售中")` which triggers bat execution. In the merged code, `selling` should be set when `stockStatus == 3` (有库存) for the target ticket.
- **Table display**: The TUI table in `show_table()` uses complex cursor positioning (`\033[s`, `\033[E`, `\033[G`). Thread-safe table updates require the mutex to protect the entire render cycle, not just individual prints.
