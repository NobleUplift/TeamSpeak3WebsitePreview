# Plan: Unit Tests for ts3websitepreview

## Context

The OGP updates **were** added to `ts3websitepreviewtest/main.c`. The test project has an identical `GetOGProperty()` function (calling libxml2 directly instead of via `pfn_*` pointers) and the same og:title/og:description/og:image extraction + fallback + message formatting logic. It is in sync with `plugin.c`.

The user asked whether the manual console-app test project could be replaced with proper unit tests on the main project. This plan does exactly that: extracts the testable pure functions out of `plugin.c` into a shared `core.c/core.h`, then rewrites `ts3websitepreviewtest` as a real unit test harness using the **Unity** framework (ThrowTheSwitch — 3 C source files, no build-system dependency).

---

## What Is Testable vs. Not

| Function | Testable? | Blocker |
|---|---|---|
| `GetURLFromMessage()` | Yes — pure C string function | None |
| `WriteMemoryCallback()` | Yes — pure callback | None |
| `BuildPreviewMessage()` (new) | Yes — pure formatting | None |
| `GetOGProperty()` | Yes (test project links libxml2 directly) | Needs XML doc fixture |
| Title-fallback XPath | Yes (same) | Needs XML doc fixture |
| `GetHTML()` | No | libcurl + network I/O |
| `ts3plugin_onTextMessageEvent()` | No | TS3 API, live connection |
| `ts3plugin_init()` | No | DLL loading, filesystem |

---

## Step 1 — Extract `core.c` / `core.h`

Create `ts3websitepreview/core.h` and `ts3websitepreview/core.c` containing the three pure/portable functions.

### `core.h` declares:
```c
struct MemoryStruct { char *memory; size_t size; };

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
const char* GetURLFromMessage(const char* message);
void BuildPreviewMessage(const char* title, const char* url,
                         const char* og_desc, const char* og_image,
                         char* out, size_t out_size);
```

### `core.c` contains:
- `WriteMemoryCallback` — moved verbatim from `plugin.c:351`
- `GetURLFromMessage` — moved verbatim from `plugin.c:369` (remove `static`)
- `BuildPreviewMessage` — **new function** extracted from the sprintf/strncat block at `plugin.c:580–601`. Signature takes the four components already computed by the caller (title, url, og_desc, og_image) and writes the formatted string into `out`.

### `plugin.c` changes:
- `#include "core.h"`
- Remove the `WriteMemoryCallback` and `GetURLFromMessage` bodies (replaced by the include)
- Replace the sprintf/strncat formatting block with a call to `BuildPreviewMessage(title, url, og_desc, og_image, newMessage, sizeof(newMessage))`
- `GetOGProperty` and `GetHTML` remain unchanged in `plugin.c` (they depend on `pfn_*` pointers)

### `ts3websitepreview.vcxproj` change:
- Add `core.c` and `core.h` as `<ClCompile>` / `<ClInclude>` items

---

## Step 2 — Add Unity Test Framework

Download the three Unity files from ThrowTheSwitch/Unity (MIT licence):
- `ts3websitepreviewtest/unity/unity.c`
- `ts3websitepreviewtest/unity/unity.h`
- `ts3websitepreviewtest/unity/unity_internals.h`

These are self-contained; no install step needed.

---

## Step 3 — Rewrite the Test Project

Replace `ts3websitepreviewtest/main.c` with three files:

### `ts3websitepreviewtest/test_url.c` — tests for `GetURLFromMessage`
```
TEST: valid [URL]...[/URL]          → returns the inner URL string
TEST: [url]...[/url] lowercase      → also accepted
TEST: text before [URL]             → returns NULL (strict prefix rule)
TEST: text after [/URL]             → returns NULL (strict suffix rule)
TEST: missing closing [/URL]        → returns NULL
TEST: empty message ""              → returns NULL
TEST: only [URL][/URL] (empty URL)  → returns empty string (or NULL — clarify)
TEST: mixed case [Url]...[/URL]     → returns NULL (neither [URL] nor [url])
```

### `ts3websitepreviewtest/test_callback.c` — tests for `WriteMemoryCallback`
```
TEST: single chunk appended correctly
TEST: multiple sequential chunks accumulate correctly
TEST: size=0 / nmemb=0 returns 0, buffer unchanged
TEST: returned value equals size * nmemb
```

### `ts3websitepreviewtest/test_message.c` — tests for `BuildPreviewMessage`
```
TEST: title + URL only → "\"TITLE\" <[URL]URL[/URL]>"
TEST: title + URL + og_desc → adds newline + description
TEST: title + URL + og_image → adds \n<img src="…">
TEST: all four fields → combined format
TEST: output truncates at out_size boundary without overflow
```

### `ts3websitepreviewtest/test_parse.c` — tests for OGP/title parsing (links libxml2 directly)

Build minimal HTML strings in memory with `htmlReadMemory`, then run the same `GetOGProperty` logic (direct calls, same as already in the test project):
```
TEST: HTML with og:title → GetOGProperty returns the value
TEST: HTML without og:title → returns NULL → fallback to <title>
TEST: HTML with og:description and og:image → both returned
TEST: malformed HTML → does not crash, title fallback used
```

### `ts3websitepreviewtest/main_test.c` — Unity runner
```c
int main(void) {
    UNITY_BEGIN();
    // call RUN_TEST for each test function
    return UNITY_END();
}
```

### `ts3websitepreviewtest.vcxproj` changes:
- Remove old `main.c`
- Add `unity/unity.c`, `test_url.c`, `test_callback.c`, `test_message.c`, `test_parse.c`, `main_test.c`
- Add `..\..\ts3websitepreview\core.c` (or link as additional source)
- Ensure include paths cover `unity/`, `../ts3websitepreview/`, and libxml2/libcurl headers

---

## Files to Create / Modify

| Path | Action |
|---|---|
| `ts3websitepreview/core.h` | Create |
| `ts3websitepreview/core.c` | Create |
| `ts3websitepreview/plugin.c` | Modify (include core.h, remove moved functions, call BuildPreviewMessage) |
| `ts3websitepreview/ts3websitepreview.vcxproj` | Modify (add core.c/core.h) |
| `ts3websitepreviewtest/unity/unity.c` | Create (Unity source) |
| `ts3websitepreviewtest/unity/unity.h` | Create (Unity header) |
| `ts3websitepreviewtest/unity/unity_internals.h` | Create (Unity internals) |
| `ts3websitepreviewtest/test_url.c` | Create |
| `ts3websitepreviewtest/test_callback.c` | Create |
| `ts3websitepreviewtest/test_message.c` | Create |
| `ts3websitepreviewtest/test_parse.c` | Create |
| `ts3websitepreviewtest/main_test.c` | Create |
| `ts3websitepreviewtest/main.c` | Delete |
| `ts3websitepreviewtest/ts3websitepreviewtest.vcxproj` | Modify (swap source files) |

---

## Verification

1. Build `ts3websitepreview` (DLL) — no regressions, same output as before.
2. Build `ts3websitepreviewtest` (console EXE).
3. Run the EXE — Unity prints pass/fail per test and exits with 0 on all-pass.
4. Manually trigger a few edge-case URLs in TS3 to confirm the plugin still sends the formatted OGP message.
