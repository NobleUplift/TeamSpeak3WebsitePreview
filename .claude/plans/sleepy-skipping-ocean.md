# Cross-platform build (Windows/Linux/macOS) + GitHub Actions CI

## Context

The repo was just restructured to CMake + the [Qt-Plugin-Template](https://github.com/Gamer92000/TeamSpeak3-Qt-Plugin-Template)
layout, but it still builds **Windows-only** and has **no CI**. The template ships a `deploy.yml`
that builds win64/win32/linux_amd64/mac and publishes a combined `.ts3_plugin` on version tags.
The user wants the same: add that CI, and make the plugin compile for Linux and macOS.

The template achieves cross-platform trivially because it's a **Qt** plugin (Qt is portable). Ours
is **C with no Qt**, and it currently uses Windows-only APIs, so this is a genuine C port, not just
new CMake targets.

### Portability audit (verified by reading the source)
- **`src/core.c`** and **`src/plugin.h`** — already cross-platform (`#else` fallbacks / dual export
  macro). No changes.
- **`src/plugin.c`** — does **not** compile off Windows: the `pfn_*` curl/libxml2 function pointers
  are declared only under `#ifdef _WIN32`, yet the call sites are unconditional; plus `DWORD`,
  `MAX_PATH`, `GetTickCount()`, `strncpy_s`/`_TRUNCATE` are used unguarded, and the settings dialog
  is Win32. This is the main port.
- **`src/settings.c`** — Win32 ini APIs (`GetPrivateProfileIntA`/`WritePrivateProfileStringA`),
  `<windows.h>`, `MAX_PATH`. Needs a portable ini fallback.
- Local Linux toolchain **has** libcurl 8.21.0 + libxml2 2.15.3 (incl. `globals.h`) → the Linux
  build (and the unit tests) are **compile/run-testable here**. macOS and MSVC are not.

### Decisions (confirmed / chosen)
- **Windows CI deps: vcpkg** (`curl[ssl]` → SChannel + `libxml2`). CMake uses `third_party/` when
  present (local dev, unchanged) and **falls back to `find_package`** when it's absent (CI/vcpkg).
- **Linux/macOS: direct-link** system libcurl + libxml2 (via `find_package(CURL)` /
  `find_package(LibXml2)`) — no `dlopen`. The Windows runtime dynamic-load model stays as-is.
- **No settings dialog off Windows** — `ts3plugin_offersConfigure()` returns 0 there; settings still
  load from a portable ini. (A Qt dialog would defeat the no-Qt design.)
- **One combined `.ts3_plugin`** with all four binaries (matches the template). So
  `deploy/package.ini` `Platforms` goes back to `win64, win32, linux_amd64, mac`. Linux/macOS ship
  just the `.so`/`.dylib` (deps are system libs); Windows still bundles `libcurl/libxml2/libiconv`
  DLLs in the `plugins/ts3websitepreview/` subdir.

## Implementation

### 1. `src/plugin.c` — portability shims
- Change the dep includes to the standard prefixed forms (resolve on Windows via `third_party/include`
  **and** on Linux/mac via system dirs): `<curl/curl.h>`, `<libxml/HTMLparser.h>`,
  `<libxml/globals.h>`, `<libxml/xpath.h>`.
- Add a `#ifndef _WIN32` compat block (after the existing `#ifdef _WIN32` string-macro block): typedef
  `DWORD`, define `MAX_PATH`/`_TRUNCATE`, a `clock_gettime`-based `GetTickCount()`, a `strncpy_s()`
  helper, and `#define pfn_<fn> <fn>` for every curl/libxml2 pointer so the unconditional call sites
  compile and bind directly to the linked libs.
- `ts3plugin_offersConfigure()` → `#ifdef _WIN32` return 1 `#else` return 0. The Win32 dynamic-load
  block, `SettingsDlgProc`, and the `DialogBox` call already sit under `#ifdef _WIN32` — leave them.

### 2. `src/settings.c` — portable ini
- Guard `<windows.h>` with `#ifdef _WIN32`; define `MAX_PATH` fallback.
- `Settings_Load`/`Settings_Save`: keep the Win32 `GetPrivateProfileIntA`/`WritePrivateProfileStringA`
  path under `#ifdef _WIN32`; add an `#else` using `stdio` — read `Key=Value` lines with `sscanf`
  (accept optional spaces), write `[Settings]` + the two keys. Same `ts3websitepreview.ini` filename.

### 3. `CMakeLists.txt` — three-platform build
- `project(... LANGUAGES C)`; `enable_language(RC)` only `if(WIN32)`. Make `PLUGIN_VERSION` a
  `CACHE STRING` so CI can pass `-DPLUGIN_VERSION=<tag>`.
- Platform suffix: `APPLE→mac`, `WIN32→win32/win64` (by `CMAKE_SIZEOF_VOID_P`), else `linux_amd64`
  (or `linux_x86`). Set `OUTPUT_NAME ts3websitepreview_${suffix}`, `PREFIX ""`, and `SUFFIX`
  `.dll`/`.so`/`.dylib` per platform — so CMake emits the final TS3 name directly (no CI rename).
- Sources: `core.c plugin.c settings.c`, appending `settings.rc` only `if(WIN32)`.
- **Windows branch**: keep MSVC defines / `MSVC_RUNTIME_LIBRARY` (/MD) / IPO / dynamic-load model
  (link no dep libs). Headers: `if(EXISTS third_party/include/curl/curl.h)` use `third_party/include`,
  `else()` `find_package(CURL/LibXml2)` for **include dirs only** (vcpkg headers; DLLs shipped by CI).
- **Linux/macOS branch**: `find_package(CURL REQUIRED)` + `find_package(LibXml2 REQUIRED)`;
  `target_link_libraries(... CURL::libcurl LibXml2::LibXml2)`; define `TS3WEBSITEPREVIEW_EXPORTS`.
- Test target (`BUILD_TESTS`): same header logic; **link** libxml2 (+curl) — on Windows the
  `third_party`/vcpkg libs, on Linux/mac `CURL::libcurl LibXml2::LibXml2` — since `test_parse.c` calls
  libxml2 directly. (Enables running the suite locally on Linux.)

### 4. `deploy/package.ini` — restore all platforms
`Platforms = win64, win32, linux_amd64, mac` (keep the `<version>` placeholder + our metadata).

### 5. `.github/workflows/deploy.yml` — new CI (adapted from the template, Qt removed)
- `on: push: tags: ['v*']`.
- **build** matrix — `windows-latest`(x64,x86), `ubuntu-latest`, `macos-latest`; checkout with
  `submodules: true`:
  - Windows: `ilammy/msvc-dev-cmd`; vcpkg `install curl[ssl] libxml2:<triplet>`; configure with the
    vcpkg toolchain + `-A x64|Win32`; collect the built DLL **and** vcpkg's `libcurl.dll` / `libxml2.dll`
    / `iconv`(→`libiconv.dll`) runtime DLLs.
  - Linux: `apt-get install -y libcurl4-openssl-dev libxml2-dev`; configure + build.
  - macOS: `brew install curl libxml2` (or system); configure + build. No `install_name_tool` (no Qt).
  - All: `-DPLUGIN_VERSION=${GITHUB_REF_NAME#v}`, `--config Release`; `upload-artifact` the
    `ts3websitepreview_*.{dll,so,dylib}` (+ Windows dep DLLs).
- **release** job: download artifacts; stage into `deploy/plugins/` (main binaries flat; Windows dep
  DLLs into `deploy/plugins/ts3websitepreview/`); `sed` `<version>`→tag in `deploy/package.ini`;
  `zip -r` the **contents** of `deploy/` → `ts3websitepreview.<tag>.ts3_plugin` (Linux `zip` gives
  forward-slash + Deflate, satisfying TS3); `softprops/action-gh-release`.

### 6. Presets & docs
- `CMakePresets.json`: add `linux` / `mac` configure+build presets (default generator, host
  `condition`); Windows presets unchanged.
- Update `README.md` + `CLAUDE.md`: cross-platform build commands, the vcpkg-vs-`third_party`
  Windows fallback, "no settings dialog off Windows", and the CI/release flow.

## Verification
- **Local (real):** `cmake -S . -B build/linux -DBUILD_TESTS=ON && cmake --build build/linux` — must
  compile `plugin.c`+`settings.c`+`core.c` clean against system libcurl/libxml2, producing
  `build/linux/out/ts3websitepreview_linux_amd64.so`. Then `ctest --test-dir build/linux` → 23 tests
  pass. This exercises the entire non-Windows port.
- **Cannot test here:** macOS build, MSVC build (vcpkg + `third_party` fallback), and the Actions
  workflow itself — CI always needs real-run iteration. These are delivered as a best-effort first
  cut; note in the summary that the first tag push will likely need CI touch-ups (esp. vcpkg DLL
  collection + the release staging paths).
- Windows regression check (by user): local `third_party` build still produces the same
  `ts3websitepreview_win64.dll` and behaves as before (dynamic load unchanged).
