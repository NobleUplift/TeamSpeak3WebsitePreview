# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A TeamSpeak 3 client plugin (C DLL) that intercepts outbound chat messages containing URLs and prepends the webpage title before resending the message to the channel.

## Target Environment

- **TeamSpeak 3 Client**: 3.6.2 (released September 2023), Qt 5.15.2
- **Plugin API version**: 26 (returned by `ts3plugin_apiVersion()`)
- **libcurl**: must be built against OpenSSL 1.1.1u (the version bundled with TS3 3.6.x)

The plugin is Windows-only (Win32 and x64 DLL). It has no dependency on Qt or other TS3-internal libraries.

## Build

Open `TeamSpeak 3 Website Preview.sln` in Visual Studio 2010+, then build via the IDE or MSBuild:

```bash
msbuild "TeamSpeak 3 Website Preview.sln" /p:Configuration=Release /p:Platform=Win32
msbuild "TeamSpeak 3 Website Preview.sln" /p:Configuration=Release /p:Platform=x64
```

Output DLLs go into `ts3websitepreview_win32.ts3_plugin/plugins/` and `ts3websitepreview_win64.ts3_plugin/plugins/`. Those directories are the installable `.ts3_plugin` packages (ZIP archives containing `package.ini` and the DLL).

Headers are vendored under `ts3websitepreview/include/` and import libs under `ts3websitepreview/lib[64]/`. The binary DLLs and `.lib` files are excluded from git (`.gitignore` excludes `*.lib`, `*.dll`, and `include/*`), so a fresh clone will not have them — they must be sourced externally.

## Tests

`ts3websitepreviewtest/` is a standalone console app (not an automated test framework) that exercises the same URL extraction → HTTP fetch → XPath title parse pipeline against a hardcoded YouTube URL. Build and run the `ts3websitepreviewtest` project directly; it prints results and pauses.

## Architecture

**Three projects in the solution:**

- **ts3websitepreview** — the plugin DLL. Implements the TeamSpeak 3 plugin API (API version 26). The only event handler that matters is `ts3plugin_onTextMessageEvent()` in `plugin.c`.
- **libiconv** — vendored character encoding library, compiled as its own DLL. Not called directly from `plugin.c`; it is an indirect dependency of libxml2/libcurl.
- **ts3websitepreviewtest** — standalone console app mirroring plugin logic for manual testing.

**Message processing flow** (`plugin.c:ts3plugin_onTextMessageEvent`):

1. Ignore messages not sent by the local user (avoids processing others' messages).
2. `GetURLFromMessage()` — extracts the URL from a message that is *exactly* `[URL]...[/URL]` (must start and end with those tags, no surrounding text).
3. `GetHTML()` — fetches the URL via libcurl into a `MemoryStruct` buffer.
4. `htmlReadMemory()` + XPath `/html/head/title` via libxml2 — extracts the page title.
5. Reconstructs the message as `"<title>" <[URL]...[/URL]>` and calls `requestSendChannelTextMsg()`.
6. `sentSelfMessage` flag prevents the resent message from triggering another fetch loop.

**Key constraints to be aware of:**

- SSL verification is disabled (`CURLOPT_SSL_VERIFYPEER = 0`). This is intentional for the current implementation but insecure.
- `GetURLFromMessage()` is strict: the entire message must be the URL tag — no prefix, no suffix. Any deviation returns NULL and skips processing.
- The XPath loop over title nodes has a `continue` and never `break`s; it processes all title nodes but only the last one survives into the output string.
- Known memory management issues exist around `xmlFree` (see git history); libxml2 cleanup paths have been attempted and partially reverted.
- The plugin is single-threaded (TS3 callback model); `TS3Functions` is a global struct set once during `ts3plugin_setFunctionPointers()`. `GetHTML()` is blocking and runs directly on that callback thread — slow URLs will stall the TS3 client UI.
- On Windows, the reconstructed message wraps the URL back in `[URL]...[/URL]` tags (using `sprintf_s`). The non-Windows path uses a raw URL instead — this matters if porting.
- `wcharToUtf8()` (Windows-only) converts the plugin name literal from UTF-16 to UTF-8 via `WideCharToMultiByte(CP_UTF8)`. All strings passed to TS3 API must be UTF-8.
