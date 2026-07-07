# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A TeamSpeak 3 client plugin that intercepts outbound chat messages containing URLs, fetches each
page's title (and optionally its Open Graph description), and resends the message with that context
prepended. It is a **Qt** plugin (C++), portable to Windows, Linux, and macOS.

## Target Environment

- **TeamSpeak 3 Client**: 3.6.2 (released September 2023), which ships **Qt 5.15.2**.
- **Plugin API version**: 26 (returned by `ts3plugin_apiVersion()`).
- **Qt**: Widgets + Network, version 5.15.x — provided by the running client (linked, **not shipped**).
  HTTP is `QNetworkAccessManager`, the settings dialog is a `QDialog`, and persistence is `QSettings`.
- **Gumbo** (`google/gumbo-parser`): the HTML5 parser behind title/OGP extraction (`src/webparse`),
  vendored as a git submodule and compiled into a **static** library — so nothing extra is shipped.

There is no libcurl and no libxml2, and the code has **no platform `#ifdef` blocks**.

Builds for **Windows** (`_win32`/`_win64` `.dll`), **Linux** (`_linux_amd64` `.so`), and **macOS**
(`_mac.dylib`). `src/core.c` is plain portable C (string logic); everything else is C++/Qt.

## Build

CMake ≥ 3.21. **Always required:** the git submodules (`git submodule update --init`, or
`git clone --recursive`) — the TS3 SDK headers (`ts3client-pluginsdk`) and the Gumbo parser
(`third_party/gumbo-parser`, compiled from source by CMake — no separate install). Plus **Qt 5.15**
(Widgets + Network); the only thing you install per platform.

- **Windows** (Visual Studio 2022): install Qt 5.15.2 (msvc2019), point CMake at it via
  `CMAKE_PREFIX_PATH`, then `cmake --preset win64 && cmake --build --preset win64` (and `win32`).
- **Linux**: `sudo apt-get install -y qtbase5-dev`, then `cmake --preset linux && cmake --build --preset linux`.
- **macOS**: `brew install qt@5`, set `CMAKE_PREFIX_PATH=$(brew --prefix qt@5)`, then
  `cmake --preset mac && cmake --build --preset mac`.

Each build emits `build/<preset>/out/ts3websitepreview_<suffix>{.dll,.so,.dylib}`. The suffix is
mandatory (TS3 ignores binaries without it) and is set via the target `OUTPUT_NAME` — no post-build
rename. `find_package(Qt5 REQUIRED COMPONENTS Widgets Network)` + `CMAKE_AUTOMOC ON` drive the Qt
build; `Q_OBJECT` classes (`ConfigDialog`, the QtTest) are moc'd automatically.

**Plugin metadata** (name/version/author/description) lives in the `project()` + `PLUGIN_*` block at
the top of `CMakeLists.txt`; `PLUGIN_VERSION` is a cache var so CI passes `-DPLUGIN_VERSION=<tag>`.
`plugin_version.h` is generated from `cmake/plugin_version.h.in` into `build/<preset>/generated/`.

**Packaging** into the installable `.ts3_plugin` is done by CI — see **Continuous Integration**.

## TS3 Plugin Package Structure

Because Qt is client-provided and nothing else is bundled, one combined `.ts3_plugin` (a ZIP) is just:

```
package.ini
plugins/
  ts3websitepreview_win64.dll        ← TS3 loads the binary matching the client OS/arch
  ts3websitepreview_win32.dll
  ts3websitepreview_linux_amd64.so
  ts3websitepreview_mac.dylib
```

- Each binary lives directly under `plugins/` and **must** carry its platform suffix.
- There is **no dependency subdirectory** — the old `plugins/ts3websitepreview/` tree (bundled
  libcurl/libxml2 DLLs) is gone.

**ZIP format requirements for `package_inst.exe`** (TS3's installer is strict): entry names must use
**forward slashes**, compression must be standard **Deflate** (no LZMA/Deflate64), the root folder must
**not be double-nested** (zip the *contents* of the staging dir, so `plugins/…` is at the zip root),
and filenames must be **strict ASCII**. The CI `release` job satisfies all of these with Linux
`zip -qq -r` over the staging contents.

**macOS Qt rpaths:** the built `.dylib` references Qt as install-time framework paths; CI runs
`install_name_tool` to rewrite those to `@rpath/libQt5*.dylib` so the client's Qt is used at load time.

**`[URL]` / emoticons / scheme:** TS3 emoticon substitution (`:)` `:D` `8)` …) applies in channel
descriptions, not chat; URLs inside `[URL]...[/URL]` are safe (`://` is not substituted). URLs sent to
chat **must** include the full scheme (`https://`/`http://`); protocol-relative `//example.com` renders
as plain text.

**Installing while TS3 is running** shows a misleading "Fail to install Add-On … retry as
Administrator?" prompt — Administrator does not help; close TS3 first. 0-byte files after install mean
a malformed ZIP (backslashes / wrong compression), not that TS3 was open.

## Continuous Integration

`.github/workflows/deploy.yml` (adapted from the Qt-Plugin-Template) builds + publishes on a `v*` tag:

- **build matrix** — `windows-latest` (x64 + x86), `ubuntu-latest`, `macos-latest`; checks out with
  submodules and installs Qt 5.15.2 via `jurplel/install-qt-action@v3` (per-platform `arch`:
  `win64_msvc2019_64` / `win32_msvc2019` / `gcc_64` / `clang_64`). Windows also sets up MSVC. Version
  comes from the tag (`-DPLUGIN_VERSION=${tag#v}`). CMake emits the suffixed filename, so no rename.
  macOS runs the `install_name_tool` Qt-rpath fix.
- **release** — merges the four binaries into `plugins/`, `sed`s `<version>` into `deploy/package.ini`,
  `zip -r`s the tree into `ts3websitepreview.<tag>.ts3_plugin`, and attaches it to a GitHub release.

CI is inherently iterative — the first tag push may need touch-ups, most likely the Qt `arch` names or
the macOS `install_name_tool` change list.

## Tests

Opt-in via `-DBUILD_TESTS=ON`; two targets, both under `ctest`:

- **`ts3websitepreview_tests`** — Unity C tests for the pure string logic in `src/core.c`
  (`GetURLFromMessage`, `BuildPreviewMessage`, `FindURLsInMessage` / inline rebuild). ~24 tests.
- **`ts3websitepreview_webparse_tests`** — a QtTest exercising `src/webparse` (title, entity decoding,
  whitespace, og:* extraction incl. attribute-order/quote variants). ~10 tests.

Both build and run on Linux with the system Qt, e.g.
`cmake --preset linux -DBUILD_TESTS=ON && cmake --build --preset linux && ctest --test-dir build/linux`.

## Architecture

**CMake targets:** `ts3websitepreview` (the plugin `MODULE`, default build target); the two test
executables above when `-DBUILD_TESTS=ON`.

**Source layout** (`src/`):
- `plugin.cpp` — the TS3 plugin API entry points (`extern "C"`, declared in `plugin.h` with the dual
  `__declspec(dllexport)` / `visibility("default")` export macro). Holds the message-handling logic,
  the Qt fetch, and wires the config dialog + settings.
- `core.c` / `core.h` — portable C: `GetURLFromMessage`, `BuildPreviewMessage`, `FindURLsInMessage`,
  `BuildMessageWithInlineTitles`. `core.h` has `extern "C"` guards so `plugin.cpp` can call it.
- `webparse.h` / `webparse.cpp` — `extractTitle()` / `extractOgProperty()` by walking the Gumbo HTML5
  parse tree (Gumbo is statically linked; `third_party/gumbo-parser` submodule, built by CMake).
- `config.h` / `config.cpp` — `ConfigDialog` (`QDialog`, two checkboxes) + `settingsLoad`/`settingsSave`
  (`QSettings` IniFormat at `<pluginPath>/ts3websitepreview.ini`).

**Message processing flow** (`src/plugin.cpp` → `ts3plugin_onTextMessageEvent`):
1. Act only on our own outbound messages (`fromID == myID`) and skip Friend/Foe-ignored ones.
2. **Use case 1** — the whole message is exactly `[URL]…[/URL]` (`GetURLFromMessage`): fetch via
   `httpGet()`, pick `og:title` else `<title>` else `(untitled)`, `BuildPreviewMessage` →
   `requestSendChannelTextMsg` as `"<title>" <[URL]…[/URL]>` (+ og:description if the setting is on).
3. **Use case 2** — URLs embedded in typed text (`FindURLsInMessage`), gated by the *Show title inline*
   setting: fetch each, `BuildMessageWithInlineTitles`, resend if changed.
4. **Echo suppression:** `sentSelfMessage` catches TS3's immediate local echo of our formatted message
   (display once, then clear the flag); a 30-second URL/message + timestamp cache
   (`lastSentURL`/`lastSentMessage`, using `QDateTime::currentMSecsSinceEpoch`) drops the later server
   echo of the *original* message without refetching.

**Networking (`httpGet`):** a per-request `QNetworkAccessManager` with `NoLessSafeRedirectPolicy` and a
browser-ish user agent, run **synchronously** via a local `QEventLoop` that quits on `finished`. Because
all QObjects are created locally, it works on whatever thread `onTextMessageEvent` runs on. SSL errors
are ignored (`ignoreSslErrors`), matching the previous `CURLOPT_SSL_VERIFYPEER=0` behaviour.

**Settings dialog:** `ts3plugin_offersConfigure()` returns `PLUGIN_OFFERS_CONFIGURE_QT_THREAD` on every
platform, and `ts3plugin_configure()` shows a `ConfigDialog` parented to the passed `qParentWidget` —
so the same dialog works everywhere (no native Win32 dialog / `.rc`).

**Key constraints:**
- SSL verification is disabled (`ignoreSslErrors`) — intentional (parity with the old behaviour) but
  insecure; Qt would otherwise validate properly.
- `GetURLFromMessage()` is strict: the entire message must be exactly the `[URL]…[/URL]` tag (no
  prefix/suffix), else it returns NULL and use case 2 is tried instead.
- `httpGet()` **blocks** the calling thread until the fetch completes (via the nested `QEventLoop`) —
  same behaviour as the old synchronous libcurl call; a slow URL can stall that thread. Real in-client
  threading (dialog on the Qt thread, network on the callback thread) is only observable in TS3 itself.
- The plugin follows the single-threaded TS3 callback model; `TS3Functions` is a global set once in
  `ts3plugin_setFunctionPointers()`.
- `webparse` parses the response with Gumbo — a lenient, spec-compliant HTML5 parser that handles
  malformed markup and decodes entities itself. The remaining limitation is charset: the bytes are
  decoded as UTF-8, so a non-UTF-8 page that doesn't declare its encoding may mis-decode.
