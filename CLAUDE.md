# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A TeamSpeak 3 client plugin (C DLL) that intercepts outbound chat messages containing URLs and prepends the webpage title before resending the message to the channel.

## Target Environment

- **TeamSpeak 3 Client**: 3.6.2 (released September 2023), Qt 5.15.2
- **Plugin API version**: 26 (returned by `ts3plugin_apiVersion()`)
- **libcurl / libxml2**: on **Windows** they are bundled and loaded at runtime (libcurl uses the
  SChannel backend — no OpenSSL); on **Linux/macOS** the system libraries are linked directly.

The plugin builds for **Windows** (Win32/x64 `.dll`), **Linux** (x86-64 `.so`), and **macOS**
(`.dylib`). It has no dependency on Qt or other TS3-internal libraries. Platform-specific code in
`src/plugin.c` / `src/settings.c` is `#ifdef _WIN32`-guarded; `src/core.c` and `src/plugin.h` are
already portable. The **settings dialog is Windows-only** (native Win32 resource dialog) — off
Windows `ts3plugin_offersConfigure()` returns 0 and settings load from a portable `.ini` fallback.

## Build

The build is **CMake-based** (CMake ≥ 3.21); there is no `.sln` (the old `.vcxproj` /
`build_plugin.bat` were removed). Presets live in `CMakePresets.json`. **Always required:** the TS3
SDK submodule — `git submodule update --init` (or `git clone --recursive`); it provides the headers
at `ts3client-pluginsdk/include` (replaced the old vendored `include/teamspeak3/`).

**Windows** (Visual Studio 2022, Desktop C++). Supply `libcurl` + `libxml2` + `iconv` (headers +
import libs + DLLs) under `third_party/` — gitignored, sourced externally, see `third_party/README.md`;
libcurl must use the SChannel backend (no OpenSSL). The VS generator is single-arch per build tree:
```bat
cmake --preset win32 && cmake --build --preset win32
cmake --preset win64 && cmake --build --preset win64
```
When `third_party/` is **absent** (i.e. CI), CMake falls back to `find_package(CURL/LibXml2)` for the
headers only — the CI workflow provisions those via **vcpkg** and ships vcpkg's DLLs. Configure from a
Windows shell (cmd/PowerShell / Developer Command Prompt), not Bash/WSL.

**Linux**: `sudo apt-get install -y libcurl4-openssl-dev libxml2-dev`, then
`cmake --preset linux && cmake --build --preset linux`.
**macOS**: `brew install libxml2` (libcurl is system), then `cmake --preset mac && cmake --build --preset mac`.

Each build emits `build/<preset>/out/ts3websitepreview_<suffix>{.dll,.so,.dylib}` — suffix is
`win32`/`win64`/`linux_amd64`/`mac`, chosen from `APPLE` / `WIN32` / `CMAKE_SIZEOF_VOID_P`. TS3
requires the suffix in the filename, so CMake sets it via `OUTPUT_NAME` (no post-build rename). On
Linux/macOS the plugin **links** system libcurl + libxml2; on Windows it links neither (runtime
dynamic load — see below).

**Packaging is not done by the local build.** CMake only compiles the per-platform binary; assembling
all four binaries + Windows dependency DLLs + `package.ini` into one installable `.ts3_plugin` ZIP is
done by CI — see **Continuous Integration** below.

**Plugin metadata (name, version, author, description) lives in `CMakeLists.txt`** — in the
`project()` call and the `PLUGIN_*` variables just below it (single source of truth; replaced the
vcxproj `PluginInfo` PropertyGroup). `PLUGIN_VERSION` is a cache var so CI can pass
`-DPLUGIN_VERSION=<tag>`. `plugin_version.h` is generated from `cmake/plugin_version.h.in` by
`configure_file` into `build/<preset>/generated/plugin_version.h` — not in the source tree, so editing
a generated copy is pointless. The plugin *package* manifest is the separate, committed
`deploy/package.ini` (template-repo style, with a `<version>` placeholder CI fills) — CI substitutes
the tag into both, so a `v*` tag push is the single source of the released version.

## TS3 Plugin Package Structure

One combined `.ts3_plugin` (a ZIP) carries every platform's binary:

```
package.ini
plugins/
  ts3websitepreview_win64.dll        ← per-platform binaries; TS3 loads the one it needs
  ts3websitepreview_win32.dll
  ts3websitepreview_linux_amd64.so
  ts3websitepreview_mac.dylib
  ts3websitepreview/                 ← Windows-only bundled deps, in ARCH subdirs (see below)
    win64/  (libcurl.dll libxml2.dll + their runtime DLLs)
    win32/  (libcurl.dll libxml2.dll + their runtime DLLs)
```

**Critical placement rules:**
- Each plugin binary lives directly under `plugins/`, **not** in the subdirectory, and **must** carry
  its platform suffix (`_win32`/`_win64`/`_linux_amd64`/`_mac`) — TS3 ignores binaries without it.
- **Windows deps go in an ARCH subdir** — `plugins/ts3websitepreview/win64/` and `.../win32/`. A single
  package needs both 32- and 64-bit `libcurl.dll`/`libxml2.dll`, which share names, so they cannot sit
  in one flat subdir. `src/plugin.c` builds the load path with `#ifdef _WIN64` → `win64`/`win32`,
  matching this layout. TS3 does not search `plugins/` for transitive deps, hence the bundling.
- **Linux/macOS bundle nothing** — the `.so`/`.dylib` link the system libcurl + libxml2.
- The exact dep DLL set is whatever **vcpkg** ships for `curl`+`libxml2` (libcurl.dll, libxml2.dll,
  and their transitive DLLs — iconv/zlib/lzma/…); CI copies the whole vcpkg `bin/` for the triplet, so
  their internal cross-references stay consistent (no manual `iconv.dll`→`libiconv.dll` rename needed,
  unlike the old hand-vendored build).

**ZIP format requirements for `package_inst.exe`:** The `.ts3_plugin` file is a ZIP archive. TS3's installer (`package_inst.exe`) is strict about two things:

1. **Entry names must use forward slashes** (`plugins/ts3websitepreview/libcurl.dll`), not backslashes. `ZipFile.CreateFromDirectory` on Windows produces backslash separators, which causes `package_inst.exe` to fail extraction of nested paths (files land in the wrong place or are silently skipped). Use `ZipArchive` with a manual entry-name loop and `.Replace('\','/')`.
2. **Compression must be standard Deflate.** LZMA, LZMA2, and Deflate64 are not supported. `CompressionLevel.Optimal` in `System.IO.Compression` produces standard Deflate and works correctly.

3. **Entry names must not double-nest the root folder.** Zip the *contents* of the staging directory, not the directory itself — `package_inst.exe` expects `plugins/...` at the zip root, not `staging_x64/plugins/...`. The `ZipArchive` loop strips the staging path prefix correctly.
4. **Filenames must be strict ASCII alphanumeric.** The extraction engine fails silently (0-byte output) on entries containing Umlauts (`ä`, `ö`, `ü`) or other non-ASCII characters; keep all file and folder names to `[A-Za-z0-9_.-]`.

The CI `release` job satisfies all four points by zipping the *contents* of the staging dir with
Linux `zip -qq -r` (forward-slash entry names + standard Deflate, ASCII-only names), and only the
built binaries + dep DLLs + `package.ini` are ever staged, so no linker artifacts leak in. (The CMake
build also emits the Windows import `.lib`/`.exp` to a separate `implib/` dir and the DLL to `out/`,
so CI never picks them up.)

**TS3 emoticon substitution applies in channel descriptions, not chat.** The sequences `:)` `:D` `8)` `;)` `:(` `:C` `:0` `:/` `:x` `:P` are replaced with emoji images in channel descriptions. In chat, URLs inside `[URL]...[/URL]` tags are safe — `://` is not substituted. URLs sent to chat **must** include the full scheme (`https://` or `http://`); protocol-relative URLs (`//example.com`) are not supported by TS3 and render as plain text without a clickable link.

**Installing while TS3 is running shows a misleading error.** `package_inst.exe` prompts "Fail to install Add-On. Do you want to retry as Administrator?" — but running as Administrator does not help. TS3 must be closed before installing; the Administrator prompt is a false error message. If 0-byte DLLs appear after a successful install, the cause is a malformed ZIP (e.g. backslash entry names, wrong compression), not TS3 being open.

## Continuous Integration

`.github/workflows/deploy.yml` (adapted from the template repo's `deploy.yml`, with Qt removed)
builds and publishes on a `v*` tag push:

- **build matrix** — `windows-latest` (x64 + x86), `ubuntu-latest`, `macos-latest`; checks out with
  submodules. Deps per platform: **Windows → vcpkg** (`curl` [SChannel] + `libxml2`; CMake uses its
  `find_package` fallback since `third_party/` is absent in CI), **Linux → apt**
  (`libcurl4-openssl-dev`, `libxml2-dev`), **macOS → brew** (`libxml2`; libcurl is system). Version
  comes from the tag via `-DPLUGIN_VERSION=${tag#v}`. CMake already emits the suffixed filename, so
  there is **no rename step** (unlike the template).
- **release** — merges all build artifacts into `plugins/`, arranges the Windows dep DLLs into the
  `plugins/ts3websitepreview/win64|win32/` arch subdirs, `sed`s `<version>` into `deploy/package.ini`,
  `zip -r`s the tree into `ts3websitepreview.<tag>.ts3_plugin`, and attaches it to a GitHub release.

CI is inherently iterative — the first tag push may need touch-ups, most likely the **vcpkg DLL set**
copied for Windows and **libxml2 discovery on macOS**.

## Tests

`test/` is a Unity-based unit test harness, opt-in via `-DBUILD_TESTS=ON`; build the
`ts3websitepreview_tests` target and run via CTest (or the console EXE directly). It prints pass/fail
per test and exits 0 on success. Tests cover `GetURLFromMessage`, `WriteMemoryCallback`,
`BuildPreviewMessage`, and OGP/XPath parsing (**32 tests total**). Because `test/test_parse.c` calls
libxml2 **directly** (not through the plugin's runtime `pfn_*` pointers), the test EXE **links**
libxml2 + curl — the vendored/vcpkg libs on Windows, the system libs (`CURL::libcurl`,
`LibXml2::LibXml2`) on Linux/macOS. It runs on any platform, e.g.:
`cmake --preset linux -DBUILD_TESTS=ON && cmake --build --preset linux && ctest --test-dir build/linux`.

## Architecture

**CMake targets:**

- **ts3websitepreview** — the plugin DLL (a CMake `MODULE` library) and the default build target. Implements the TeamSpeak 3 plugin API (API version 26). The only event handler that matters is `ts3plugin_onTextMessageEvent()` in `src/plugin.c`.
- **ts3websitepreview_tests** — Unity unit test harness (built only when `-DBUILD_TESTS=ON`).

There is no local packaging target — building the `.ts3_plugin` is deferred to CI (see the packaging section above).

**Source layout:** plugin sources are under `src/` (`core.c/h`, `plugin.c/h`, `settings.c/h`, `settings.rc`, `resource.h`); tests under `test/`; the TS3 SDK is the `ts3client-pluginsdk/` submodule. curl/libxml2/iconv come from `third_party/` on Windows (or vcpkg in CI) and from the system on Linux/macOS. `core.c` and `plugin.h` are portable; the Win32-specific code in `plugin.c`/`settings.c` is `#ifdef _WIN32`-guarded, with a `#ifndef _WIN32` compat block in `plugin.c` (typedefs for `DWORD`/`MAX_PATH`, a `clock_gettime` `GetTickCount`, a `strncpy_s` helper, and `#define pfn_<fn> <fn>` so the runtime-pointer call sites bind straight to the linked libs).

**Message processing flow** (`src/plugin.c` → `ts3plugin_onTextMessageEvent`):

1. Ignore messages not sent by the local user (avoids processing others' messages).
2. `GetURLFromMessage()` — extracts the URL from a message that is *exactly* `[URL]...[/URL]` (must start and end with those tags, no surrounding text).
3. `GetHTML()` — fetches the URL via libcurl into a `MemoryStruct` buffer.
4. `htmlReadMemory()` + XPath `/html/head/title` via libxml2 — extracts the page title.
5. Reconstructs the message as `"<title>" <[URL]...[/URL]>` and calls `requestSendChannelTextMsg()`.
6. `sentSelfMessage` flag prevents the resent message from triggering another fetch loop.

**Dynamic loading (Windows only):**

On **Linux/macOS** libcurl + libxml2 are linked directly (the `pfn_*` names are macros for the real
functions), so this whole section is Windows-specific.

On **Windows** libcurl and libxml2 are loaded at runtime via `LoadLibraryExW` in `ts3plugin_init()` rather than via static import. This is required because:

1. The dependency DLLs live in `plugins\ts3websitepreview\<arch>\` (an arch subdir not in Windows' default DLL search path; `<arch>` is `win64`/`win32`, picked in `plugin.c` via `#ifdef _WIN64`).
2. libxml2 cannot be delay-loaded — it exports `xmlFree` as a data symbol (`XMLPUBVAR xmlFreeFunc xmlFree`), and MSVC's linker refuses to delay-load DLLs that export data symbols (`LNK1194`).

The load sequence in `ts3plugin_init()`:
1. `GetModuleHandleExW` + `GetModuleFileNameW` on the plugin's own function address to find the absolute path of the plugin DLL.
2. Construct `<plugin_dir>\ts3websitepreview\<arch>\libcurl.dll` and load it with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`.
3. Load libxml2 similarly; `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` ensures libxml2 finds its own dependency DLLs (iconv, etc.) in the same subdirectory without `SetDllDirectory`.
4. Resolve all function pointers via `GetProcAddress` into `pfn_*` static variables.
5. `xmlFree` requires a double-dereference: `GetProcAddress` returns the address of the variable, not the function — `pfn_xmlFree = *((pfnXmlFree_t*)GetProcAddress(hLibxml2, "xmlFree"))`.

All libcurl and libxml2 calls in `GetHTML()` and `ts3plugin_onTextMessageEvent()` go through the `pfn_*` function pointer variables. `FreeLibrary` is called for both in `ts3plugin_shutdown()`.

**Key constraints to be aware of:**

- SSL verification is disabled (`CURLOPT_SSL_VERIFYPEER = 0`). This is intentional for the current implementation but insecure.
- `GetURLFromMessage()` is strict: the entire message must be the URL tag — no prefix, no suffix. Any deviation returns NULL and skips processing.
- The XPath loop over title nodes has a `continue` and never `break`s; it processes all title nodes but only the last one survives into the output string.
- The plugin is single-threaded (TS3 callback model); `TS3Functions` is a global struct set once during `ts3plugin_setFunctionPointers()`. `GetHTML()` is blocking and runs directly on that callback thread — slow URLs will stall the TS3 client UI.
- Message reconstruction is unified in `BuildPreviewMessage()` (`src/core.c`) and wraps the URL in `[URL]...[/URL]` on **all** platforms; only the optional description-append differs by `#ifdef` (`strncat_s` on Windows, `strncat` elsewhere).
- The settings dialog is Windows-only. `ts3plugin_offersConfigure()` returns 0 off Windows and `ts3plugin_configure()` is an empty stub there; settings still load via `Settings_Load()` (a portable `stdio` INI reader in the `#else` branch of `src/settings.c`).
- `wcharToUtf8()` (Windows-only) converts the plugin name literal from UTF-16 to UTF-8 via `WideCharToMultiByte(CP_UTF8)`. All strings passed to TS3 API must be UTF-8.
- `getPluginPath` (in `TS3Functions`) takes 3 parameters as of API 23 — `(char* path, size_t maxLen, const char* pluginID)`. Using the old 2-parameter signature corrupts the stack on x86.
- `sentSelfMessage` suppression: When the plugin sends a formatted message via `requestSendChannelTextMsg`, TS3 immediately fires `onTextMessageEvent` again for that message (local echo). `sentSelfMessage=1` is set before the send; the `else` branch catches the local echo, resets `sentSelfMessage=0`, and returns `0` (display). This causes the formatted message to appear exactly once via the local echo. If the server later echoes the formatted message back, it fails the URL check (formatted message doesn't start with `[URL]`) and returns `0` again — harmless double-display is the worst case, not a loop.
- `lastSentURL` deduplication: TS3 also echoes the *original* `[URL]...[/URL]` message back from the server (~5s later with `sentSelfMessage=0`), which would re-trigger fetching. A 30-second URL+timestamp cache (`lastSentURL` / `lastSentURLTick`) detects this echo and returns `1` (suppress) without refetching.
