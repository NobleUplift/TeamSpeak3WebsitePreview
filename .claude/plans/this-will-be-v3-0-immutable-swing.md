# v3.0 Plan: Dual-Mode URL Titling

## Context

v2.0 only handled the case where a user's entire message was a bare `[URL]...[/URL]` tag. v3.0
adds a second mode: when a URL is embedded inside a larger typed message, the plugin fetches each
URL's title and splices it inline. og:description is never used in this inline mode; only the
bare page title is inserted. A new setting gates this behaviour. The `show_description` setting
already defaults to `1` — no change needed there.

---

## Files to Change

| File | Change |
|------|--------|
| `ts3websitepreview/plugin_version.h` | Bump to `"3.0"`, update description text |
| `ts3websitepreview/settings.h` | Add `int show_title_inline` to `PluginSettings` |
| `ts3websitepreview/settings.c` | Default `{ 1, 1 }`, load/save `ShowTitleInline` |
| `ts3websitepreview/resource.h` | Add `IDC_CHECK_TITLE_INLINE 202` |
| `ts3websitepreview/settings.rc` | Add second checkbox; increase dialog height to 88; move buttons to y=62 |
| `ts3websitepreview/plugin.c` | Update `SettingsDlgProc`; restructure `ts3plugin_onTextMessageEvent`; add `lastSentMessage`/`lastSentMessageTick` globals |
| `ts3websitepreview/core.h` | Declare `FindURLsInMessage`, `BuildMessageWithInlineTitles` |
| `ts3websitepreview/core.c` | Implement both new functions |
| `ts3websitepreviewtest/test_inline.c` | New — tests for both new functions |
| `ts3websitepreviewtest/main_test.c` | Add `RunInlineTests()` call |

---

## Step-by-Step Implementation

### 1. `plugin_version.h`

```c
#define PLUGIN_VERSION_STR "3.0"
#define PLUGIN_DESCRIPTION "Fetches the title (and optionally description) of URLs sent in chat. Works for URL-only messages and for URLs embedded in typed messages."
```

### 2. `settings.h`

```c
typedef struct {
    int show_description;
    int show_title_inline;
} PluginSettings;
```

### 3. `settings.c`

```c
PluginSettings g_settings = { 1, 1 };
```

In `Settings_Load`: add
```c
g_settings.show_title_inline = GetPrivateProfileIntA("Settings", "ShowTitleInline", 1, ini);
```

In `Settings_Save`: add
```c
snprintf(val, sizeof(val), "%d", g_settings.show_title_inline);
WritePrivateProfileStringA("Settings", "ShowTitleInline", val, ini);
```

### 4. `resource.h`

```c
#define IDD_SETTINGS          101
#define IDC_CHECK_DESCRIPTION 201
#define IDC_CHECK_TITLE_INLINE  202
```

### 5. `settings.rc`

```rc
IDD_SETTINGS DIALOGEX 0, 0, 220, 88
...
BEGIN
    AUTOCHECKBOX "Show page description in chat",     IDC_CHECK_DESCRIPTION, 10, 12, 200, 10
    AUTOCHECKBOX "Show URL title in typed messages",  IDC_CHECK_TITLE_INLINE,  10, 28, 200, 10
    DEFPUSHBUTTON "OK",     IDOK,     110, 62, 45, 14
    PUSHBUTTON    "Cancel", IDCANCEL, 160, 62, 45, 14
END
```

### 6. `plugin.c` — `SettingsDlgProc` (lines 303–323)

`WM_INITDIALOG`: also set `IDC_CHECK_TITLE_INLINE` from `g_settings.show_title_inline`.

`WM_COMMAND / IDOK`: also read `IDC_CHECK_TITLE_INLINE` and store into `g_settings.show_title_inline`.

### 7. `plugin.c` — new globals (near line 93)

```c
static char  lastSentMessage[2048];
static DWORD lastSentMessageTick;
```

### 8. `plugin.c` — `ts3plugin_onTextMessageEvent` restructure

Replace the current early-return-on-null-URL block (lines 509–512) with a fall-through into
use case 2. New shape of the `!sentSelfMessage` branch:

```
if (!sentSelfMessage) {

    // ── Use case 1: entire message is [URL]...[/URL] ─────────────────────
    url = GetURLFromMessage(message);
    if (url != NULL) {
        // 30-s dedup (existing lastSentURL check) → return 1 if matched
        // GetHTML, parse, BuildPreviewMessage (with og:description gated by show_description)
        // strncpy_s(lastSentURL, ...), sentSelfMessage = 1, send, return 1
    }

    // ── Use case 2: URL(s) embedded in a typed message ───────────────────
    if (g_settings.show_title_inline) {
        // 30-s dedup against lastSentMessage
        DWORD now = GetTickCount();
        if (lastSentMessage[0] && strcmp(message, lastSentMessage) == 0
                && (now - lastSentMessageTick) < 30000U) {
            return 1;   // suppress server echo of the original
        }

        char* urls[5] = {0};
        int url_count = FindURLsInMessage(message, urls, 5);

        if (url_count > 0) {
            const char* titles[5] = {0};
            // for each url: GetHTML → htmlReadMemory → XPath title (no og:desc)
            // titles[i] = malloc'd string or NULL on failure

            char newMessage[4096];
            BuildMessageWithInlineTitles(message, (const char**)urls, titles,
                                         url_count, newMessage, sizeof(newMessage));

            // free urls[i] and titles[i]

            // Only send if the message actually changed
            if (strcmp(newMessage, message) != 0) {
                strncpy_s(lastSentMessage, sizeof(lastSentMessage), message, _TRUNCATE);
                lastSentMessageTick = GetTickCount();
                sentSelfMessage = 1;
                requestSendChannelTextMsg(..., newMessage, ...);
                return 1;
            }
        }
    }

    return 0;

} else {
    sentSelfMessage = 0;
    return 0;
}
```

Key: `sentSelfMessage` is shared by both use cases — the first local echo of our sent message
is always caught the same way.

### 9. `core.h` — new declarations

```c
#define MAX_URLS_PER_MESSAGE 5

int  FindURLsInMessage(const char* message, char** urls_out, int max_urls);
void BuildMessageWithInlineTitles(const char* message,
                                  const char** urls, const char** titles,
                                  int url_count, char* out, size_t out_size);
```

### 10. `core.c` — `FindURLsInMessage`

Scans for `[URL]`/`[url]` … `[/URL]`/`[/url]` pairs. For each pair, heap-allocates the URL
string (caller frees). Returns count found (≤ max_urls). Advances past `[/URL]` after each match.

### 11. `core.c` — `BuildMessageWithInlineTitles`

Walks `message` character by character. When a `[URL]...[/URL]` tag is encountered:
- Look up the URL in the `urls` array.
- If found and the matching `titles[i]` is non-NULL, emit `"<title>" <[URL]url[/URL]>`.
- Otherwise copy the tag verbatim.

All writes are bounded by `out_size`; result is always NUL-terminated.

### 12. `test_inline.c` (new file)

Tests for `FindURLsInMessage`:
- No URLs in plain text → 0
- Exact URL-only message → 1, correct URL extracted
- URL embedded in text → 1, correct URL extracted
- Two URLs in message → 2, both correct

Tests for `BuildMessageWithInlineTitles`:
- Single URL replaced with title
- Two URLs, different titles
- NULL title → original tag preserved verbatim
- Output does not overflow small buffer

### 13. `main_test.c`

Add `void RunInlineTests(void);` declaration and `RunInlineTests();` call in `main`.

---

## Deduplication Summary

| Use Case | Dedup key stored | Dedup var | Return on hit |
|----------|-----------------|-----------|---------------|
| 1 (URL-only) | extracted URL string | `lastSentURL` / `lastSentURLTick` | `1` |
| 2 (inline) | full original message text | `lastSentMessage` / `lastSentMessageTick` | `1` |

Both windows are 30 seconds. Both share the `sentSelfMessage` flag for the local echo of the
*sent* formatted message.

---

## Verification

1. Build both Win32 and x64 via `build_plugin.bat` — zero errors/warnings.
2. Build and run the test harness — all tests pass (including new inline tests).
3. Install and test manually in TS3:
   - **Use case 1**: Send `https://example.com` alone → titled message appears once, no loop.
   - **Use case 2 (enabled)**: Send `"Check https://example.com please"` → titled inline message appears once.
   - **Use case 2 (two URLs)**: Send a message with two URLs → both titles appear inline.
   - **Use case 2 (disabled)**: Uncheck "Show URL title in typed messages" → mixed messages pass through unchanged.
   - **Settings persist**: Close and reopen TS3; settings retain their values.
