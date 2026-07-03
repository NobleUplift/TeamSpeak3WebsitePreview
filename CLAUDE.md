# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A TeamSpeak 3 client plugin (C DLL) that intercepts outbound chat messages containing URLs and prepends the webpage title before resending the message to the channel.

## Target Environment

- **TeamSpeak 3 Client**: 3.6.2 (released September 2023), Qt 5.15.2
- **Plugin API version**: 26 (returned by `ts3plugin_apiVersion()`)
- **libcurl**: built with the SChannel backend (Windows-native TLS) — no OpenSSL dependency

The plugin is Windows-only (Win32 and x64 DLL). It has no dependency on Qt or other TS3-internal libraries.

## Build

Requires **Visual Studio 2022** (toolset v143). The solution file name contains spaces, so use `build_plugin.bat` or target the `.vcxproj` directly:

```bat
build_plugin.bat
```

Or via MSBuild directly:

```bat
MSBuild.exe ts3websitepreview\ts3websitepreview.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0
MSBuild.exe ts3websitepreview\ts3websitepreview.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0
```

Do not try to build the `.sln` via Bash/shell — the filename `TeamSpeak 3 Website Preview.sln` has spaces that cause `MSB1008: Only one project can be specified` errors. Use the `.vcxproj` directly or run `build_plugin.bat` via `cmd /c build_plugin.bat`.

The `/p:PlatformToolset=v143` and `/p:WindowsTargetPlatformVersion=10.0` overrides are required even though `v143` is already in the `.vcxproj` — without them, some MSBuild invocations fall back to an unavailable older toolset.

Output DLLs:
- `ts3websitepreview_win32.ts3_plugin\plugins\ts3websitepreview_win32.dll`
- `ts3websitepreview_win64.ts3_plugin\plugins\ts3websitepreview_win64.dll`

Headers are vendored under `ts3websitepreview/include/` and import libs under `ts3websitepreview/lib[64]/`. The binary DLLs and `.lib` files are excluded from git (`.gitignore` excludes `*.lib`, `*.dll`, and `include/*`), so a fresh clone will not have them — they must be sourced externally.

## TS3 Plugin Package Structure

Each `.ts3_plugin` directory (which TS3 installs as a ZIP) must be laid out as follows:

```
ts3websitepreview_win64.ts3_plugin\
  package.ini
  plugins\
    ts3websitepreview_win64.dll   ← main plugin DLL (must have _win32/_win64 suffix)
    ts3websitepreview\            ← dependency subdirectory (same name as plugin base)
      libcurl.dll
      libxml2.dll
      libiconv.dll
```

**Critical placement rules:**
- The plugin DLL itself lives directly under `plugins\`, **not** in the subdirectory.
- The plugin DLL **must** be named `ts3websitepreview_win32.dll` / `ts3websitepreview_win64.dll` — TS3 ignores DLLs without the architecture suffix.
- All dependency DLLs (`libcurl`, `libxml2`, `libiconv`) live in `plugins\ts3websitepreview\` (the subdirectory matching the plugin base name). TS3 does not search `plugins\` for transitive dependencies.
- Win32 also needs `zlib1.dll` in the subdirectory if the libcurl/libxml2 builds require it (x64 builds linked it statically).
- `libiconv.dll` must be named exactly `libiconv.dll`, not `iconv.dll` — libxml2.dll imports it by that exact name.

The `libiconv` project in the solution builds to this subdirectory automatically for Release configs.

## Tests

`ts3websitepreviewtest/` is a standalone console app (not an automated test framework) that exercises the same URL extraction → HTTP fetch → XPath title parse pipeline against a hardcoded YouTube URL. Build and run the `ts3websitepreviewtest` project directly; it prints results and pauses.

## Architecture

**Three projects in the solution:**

- **ts3websitepreview** — the plugin DLL. Implements the TeamSpeak 3 plugin API (API version 26). The only event handler that matters is `ts3plugin_onTextMessageEvent()` in `plugin.c`.
- **libiconv** — vendored character encoding library, compiled as its own DLL. Not called directly from `plugin.c`; it is an indirect dependency of libxml2.
- **ts3websitepreviewtest** — standalone console app mirroring plugin logic for manual testing.

**Message processing flow** (`plugin.c:ts3plugin_onTextMessageEvent`):

1. Ignore messages not sent by the local user (avoids processing others' messages).
2. `GetURLFromMessage()` — extracts the URL from a message that is *exactly* `[URL]...[/URL]` (must start and end with those tags, no surrounding text).
3. `GetHTML()` — fetches the URL via libcurl into a `MemoryStruct` buffer.
4. `htmlReadMemory()` + XPath `/html/head/title` via libxml2 — extracts the page title.
5. Reconstructs the message as `"<title>" <[URL]...[/URL]>` and calls `requestSendChannelTextMsg()`.
6. `sentSelfMessage` flag prevents the resent message from triggering another fetch loop.

**Dynamic loading (Windows):**

libcurl and libxml2 are loaded at runtime via `LoadLibraryExW` in `ts3plugin_init()` rather than via static import. This is required because:

1. The dependency DLLs live in `plugins\ts3websitepreview\` (a subdirectory not in Windows' default DLL search path).
2. libxml2 cannot be delay-loaded — it exports `xmlFree` as a data symbol (`XMLPUBVAR xmlFreeFunc xmlFree`), and MSVC's linker refuses to delay-load DLLs that export data symbols (`LNK1194`).

The load sequence in `ts3plugin_init()`:
1. `GetModuleHandleExW` + `GetModuleFileNameW` on the plugin's own function address to find the absolute path of the plugin DLL.
2. Construct `<plugin_dir>\ts3websitepreview\libcurl.dll` and load it with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`.
3. Load libxml2 similarly; `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` ensures libxml2 finds libiconv in the same subdirectory without `SetDllDirectory`.
4. Resolve all function pointers via `GetProcAddress` into `pfn_*` static variables.
5. `xmlFree` requires a double-dereference: `GetProcAddress` returns the address of the variable, not the function — `pfn_xmlFree = *((pfnXmlFree_t*)GetProcAddress(hLibxml2, "xmlFree"))`.

All libcurl and libxml2 calls in `GetHTML()` and `ts3plugin_onTextMessageEvent()` go through the `pfn_*` function pointer variables. `FreeLibrary` is called for both in `ts3plugin_shutdown()`.

**Key constraints to be aware of:**

- SSL verification is disabled (`CURLOPT_SSL_VERIFYPEER = 0`). This is intentional for the current implementation but insecure.
- `GetURLFromMessage()` is strict: the entire message must be the URL tag — no prefix, no suffix. Any deviation returns NULL and skips processing.
- The XPath loop over title nodes has a `continue` and never `break`s; it processes all title nodes but only the last one survives into the output string.
- The plugin is single-threaded (TS3 callback model); `TS3Functions` is a global struct set once during `ts3plugin_setFunctionPointers()`. `GetHTML()` is blocking and runs directly on that callback thread — slow URLs will stall the TS3 client UI.
- On Windows, the reconstructed message wraps the URL back in `[URL]...[/URL]` tags (using `sprintf_s`). The non-Windows path uses a raw URL instead — this matters if porting.
- `wcharToUtf8()` (Windows-only) converts the plugin name literal from UTF-16 to UTF-8 via `WideCharToMultiByte(CP_UTF8)`. All strings passed to TS3 API must be UTF-8.
- `getPluginPath` (in `TS3Functions`) takes 3 parameters as of API 23 — `(char* path, size_t maxLen, const char* pluginID)`. Using the old 2-parameter signature corrupts the stack on x86.
