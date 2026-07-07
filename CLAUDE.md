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

The build is **CMake-based** (CMake ≥ 3.21) with **Visual Studio 2022** (Desktop C++ workload) as
the generator. There is no `.sln` — the old `.vcxproj` / `build_plugin.bat` were removed in favour of
`CMakeLists.txt` + `CMakePresets.json`.

**Prerequisites (a fresh clone has neither):**
1. **TS3 SDK submodule** — `git submodule update --init` (or `git clone --recursive`). Provides the
   TS3 headers at `ts3client-pluginsdk/include` (this replaced the old vendored `include/teamspeak3/`).
2. **Vendored native deps** — `libcurl`, `libxml2`, `iconv` (headers + import libs + DLLs) under
   `third_party/`, sourced externally. See `third_party/README.md` for the exact layout. libcurl must
   use the SChannel backend (no OpenSSL). These are gitignored (`.gitignore` excludes `/third_party/`,
   `*.dll`, `*.lib`), so a fresh clone will not have them.

**Build both architectures** (the VS generator is single-arch per build tree, so each arch gets its
own configure + build; presets live in `CMakePresets.json`):

```bat
cmake --preset win32 && cmake --build --preset win32
cmake --preset win64 && cmake --build --preset win64
```

Each build produces the plugin DLL at `build\<arch>\out\ts3websitepreview_<arch>.dll`. Equivalent
explicit form:

```bat
cmake -S . -B build\win64 -G "Visual Studio 17 2022" -A x64
cmake --build build\win64 --config Release
```
(use `-A Win32` for the 32-bit build). Configure from a Windows shell (cmd/PowerShell / Developer
Command Prompt), not Bash/WSL, when using the VS generator.

**Packaging is not done by the local build.** CMake only compiles the plugin DLL; assembling the DLL
+ dependency DLLs + `package.ini` into the installable `.ts3_plugin` ZIP is deferred to CI (a GitHub
Actions workflow, to be added — mirroring the template repo, whose `deploy.yml` handles packaging).
The `deploy/` directory holds the scaffolding for it: `package.ini` (the manifest — carrying a
`<version>` placeholder for CI to substitute, matching the template repo's convention) and
`plugins/.gitkeep`. Until CI exists, package by hand per the structure below (replace `<version>`
with the real version, e.g. `3.0`).

**Plugin metadata (name, version, author, description) lives in `CMakeLists.txt`** — in the
`project()` call and the `PLUGIN_*` variables just below it (single source of truth; replaced the
vcxproj `PluginInfo` PropertyGroup). `plugin_version.h` is generated from `cmake/plugin_version.h.in`
by `configure_file` into `build\<arch>\generated\plugin_version.h` — it is no longer in the source
tree, so editing a generated copy is pointless. The plugin *package* manifest is the separate,
committed `deploy/package.ini` (template-repo style, with a `<version>` placeholder CI fills) — it is
not driven from the `PLUGIN_*` vars, so keep the two in sync when bumping the version. The output DLL
gets its mandatory `_win32`/`_win64` suffix via the target `OUTPUT_NAME` (arch detected from
`CMAKE_SIZEOF_VOID_P`).

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

The packaging step (CI) copies `third_party/lib/iconv.dll` (Win32) or `third_party/lib64/iconv.dll` (x64) into the staging directory **renamed to `libiconv.dll`**.

**ZIP format requirements for `package_inst.exe`:** The `.ts3_plugin` file is a ZIP archive. TS3's installer (`package_inst.exe`) is strict about two things:

1. **Entry names must use forward slashes** (`plugins/ts3websitepreview/libcurl.dll`), not backslashes. `ZipFile.CreateFromDirectory` on Windows produces backslash separators, which causes `package_inst.exe` to fail extraction of nested paths (files land in the wrong place or are silently skipped). Use `ZipArchive` with a manual entry-name loop and `.Replace('\','/')`.
2. **Compression must be standard Deflate.** LZMA, LZMA2, and Deflate64 are not supported. `CompressionLevel.Optimal` in `System.IO.Compression` produces standard Deflate and works correctly.

3. **Entry names must not double-nest the root folder.** Zip the *contents* of the staging directory, not the directory itself — `package_inst.exe` expects `plugins/...` at the zip root, not `staging_x64/plugins/...`. The `ZipArchive` loop strips the staging path prefix correctly.
4. **Filenames must be strict ASCII alphanumeric.** The extraction engine fails silently (0-byte output) on entries containing Umlauts (`ä`, `ö`, `ü`) or other non-ASCII characters; keep all file and folder names to `[A-Za-z0-9_.-]`.

The proven approach (used by the original `build_plugin.bat` post-build and to be reused by CI) stages only the plugin DLL plus the three dependency DLLs, then zips via .NET `System.IO.Compression.ZipArchive` filtered to `.dll`/`.ini` files only — which also excludes linker artifacts (`.exp`, `.lib`) that `ZipFile.CreateFromDirectory` would otherwise include. (The CMake build already emits the plugin's own import lib and `.exp` to a separate `implib/` output dir, and the DLL to `out/`, so they never sit next to each other.)

**TS3 emoticon substitution applies in channel descriptions, not chat.** The sequences `:)` `:D` `8)` `;)` `:(` `:C` `:0` `:/` `:x` `:P` are replaced with emoji images in channel descriptions. In chat, URLs inside `[URL]...[/URL]` tags are safe — `://` is not substituted. URLs sent to chat **must** include the full scheme (`https://` or `http://`); protocol-relative URLs (`//example.com`) are not supported by TS3 and render as plain text without a clickable link.

**Installing while TS3 is running shows a misleading error.** `package_inst.exe` prompts "Fail to install Add-On. Do you want to retry as Administrator?" — but running as Administrator does not help. TS3 must be closed before installing; the Administrator prompt is a false error message. If 0-byte DLLs appear after a successful install, the cause is a malformed ZIP (e.g. backslash entry names, wrong compression), not TS3 being open.

## Tests

`test/` is a Unity-based unit test harness. It is an opt-in CMake target — configure with
`-DBUILD_TESTS=ON`, build the `ts3websitepreview_tests` target, then run via `ctest -C Release` (or
run the console EXE directly). It prints pass/fail for each test and exits with 0 on success. Tests
cover `GetURLFromMessage`, `WriteMemoryCallback`, `BuildPreviewMessage`, and OGP/XPath parsing (23
tests total). Unlike the plugin, the test EXE **links** the `libxml2` + `iconv` (+ `zlib1` on Win32)
import libs, because `test/test_parse.c` calls libxml2 directly rather than through the runtime
`pfn_*` pointers.

## Architecture

**CMake targets:**

- **ts3websitepreview** — the plugin DLL (a CMake `MODULE` library) and the default build target. Implements the TeamSpeak 3 plugin API (API version 26). The only event handler that matters is `ts3plugin_onTextMessageEvent()` in `src/plugin.c`.
- **ts3websitepreview_tests** — Unity unit test harness (built only when `-DBUILD_TESTS=ON`).

There is no local packaging target — building the `.ts3_plugin` is deferred to CI (see the packaging section above).

**Source layout:** plugin sources are under `src/` (`core.c/h`, `plugin.c/h`, `settings.c/h`, `settings.rc`, `resource.h`); tests under `test/`; the TS3 SDK is the `ts3client-pluginsdk/` submodule; curl/libxml2/iconv live under `third_party/`.

**Message processing flow** (`src/plugin.c` → `ts3plugin_onTextMessageEvent`):

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
- `sentSelfMessage` suppression: When the plugin sends a formatted message via `requestSendChannelTextMsg`, TS3 immediately fires `onTextMessageEvent` again for that message (local echo). `sentSelfMessage=1` is set before the send; the `else` branch catches the local echo, resets `sentSelfMessage=0`, and returns `0` (display). This causes the formatted message to appear exactly once via the local echo. If the server later echoes the formatted message back, it fails the URL check (formatted message doesn't start with `[URL]`) and returns `0` again — harmless double-display is the worst case, not a loop.
- `lastSentURL` deduplication: TS3 also echoes the *original* `[URL]...[/URL]` message back from the server (~5s later with `sentSelfMessage=0`), which would re-trigger fetching. A 30-second URL+timestamp cache (`lastSentURL` / `lastSentURLTick`) detects this echo and returns `1` (suppress) without refetching.
