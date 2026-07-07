# Rewrite on Qt — drop libcurl + libxml2 + all Win32 code

## Context

The plugin works cross-platform now, but only via **many `#ifdef _WIN32` blocks** — which the user
dislikes. The root cause is the deliberate *no-Qt* design: it hand-rolls HTTP (libcurl, loaded via a
~40-line Windows `LoadLibraryExW` block), HTML parsing (libxml2), a native Win32 settings dialog, a
Win32 `.ini`, and `wchar`/`strncpy_s`/`GetTickCount` shims. The [Qt-Plugin-Template](https://github.com/Gamer92000/TeamSpeak3-Qt-Plugin-Template)
has almost no `#ifdef`s because **Qt** does the portable lifting — and the TS3 3.6.2 client already
ships Qt 5.15.2, so Qt is available to the plugin.

**Decision (confirmed):** go Qt like the template, and **drop libxml2 too** — parse with Qt. End
state: **zero platform `#ifdef`s, zero native dependencies** (pure Qt; Qt provided by the client), and
the `.ts3_plugin` is literally just the four platform binaries + `package.ini` (no bundled DLLs, no
`third_party/`, no dynamic loading).

**Locally verifiable:** Qt5 Widgets + Network **5.15.19** is installed here (`moc`,
`Qt5WidgetsConfig.cmake`) — so the plugin and the parser tests compile *and run* on Linux.

## What changes

- **Language:** the plugin becomes **C++** (`plugin.cpp`) so it can use Qt. `core.c` stays C (pure
  string logic — already portable and tested); `core.h` gets `extern "C"` guards.
- **Fetch:** `QNetworkAccessManager` (redirects on, user-agent), run synchronously via a local
  `QEventLoop` per request — replaces `GetHTML()`, libcurl, and the entire Windows dynamic-load block.
- **Parse:** `QRegularExpression` over the response for `<title>` and `og:title/description/image` —
  new `src/webparse.{h,cpp}` (`QString` in/out), replaces the libxml2 `htmlReadMemory`+XPath code.
- **Settings dialog:** a `QDialog` (`src/config.{h,cpp}`, two checkboxes + OK/Cancel, built
  programmatically — no `.ui`), mirroring the template's `config` class. Shown from
  `ts3plugin_configure()`; `ts3plugin_offersConfigure()` returns `PLUGIN_OFFERS_CONFIGURE_QT_THREAD`
  on every platform.
- **Settings storage:** `QSettings(IniFormat)` at the plugin path (same `ts3websitepreview.ini`
  filename) — replaces the Win32 `GetPrivateProfileInt`/`WritePrivateProfile` code.
- **`ts3plugin_name()`** just returns `PLUGIN_NAME` (already UTF-8) — drops `wcharToUtf8` + `PLUGIN_NAME_W`.

## Files

**Removed:** `src/settings.c`, `src/settings.h`, `src/settings.rc`, `src/resource.h` (→ Qt dialog +
`QSettings`); `third_party/README.md` and the `third_party/` gitignore rules; `test/test_callback.c`
(tested the curl write-callback — gone) and `test/test_parse.c` (tested libxml2 XPath — gone).

**New:**
- `src/plugin.cpp` (replaces `src/plugin.c`) — C++; TS3 API entry points wrapped `extern "C"`; the
  `onTextMessageEvent` flow reused as-is but fetch=Qt, parse=`webparse`, and it calls into `core.c`
  for URL extraction / message building.
- `src/config.h` + `src/config.cpp` — the `QDialog` settings dialog (`Q_OBJECT`, `QSettings`).
- `src/webparse.h` + `src/webparse.cpp` — `extractTitle()` / `extractOgProperty()` via `QRegularExpression`.
- `test/test_webparse.cpp` — a small **QtTest** exercising the parser on sample HTML (well-formed,
  missing-og, malformed) so parse coverage isn't lost.

**Changed:**
- `src/core.c` — unchanged logic; **remove `WriteMemoryCallback`** (curl-only). Keep
  `GetURLFromMessage`, `BuildPreviewMessage`, `FindURLsInMessage`, `BuildMessageWithInlineTitles`.
  `src/core.h` — add `extern "C"` guards.
- `src/plugin.h` — keep the dual `PLUGINS_EXPORTDLL` macro and the `ts3plugin_*` declarations; drop
  nothing else structural.
- `CMakeLists.txt` — `find_package(Qt5 REQUIRED COMPONENTS Widgets Network)`, `CMAKE_AUTOMOC ON`,
  `CMAKE_CXX_STANDARD 17`; the target is now C+C++ (`core.c` compiled as C); `target_link_libraries(...
  Qt5::Widgets Qt5::Network)`; **delete all `third_party` / curl / libxml2 / RC / dynamic-load logic**.
  Keep the platform suffix/extension naming and `plugin_version.h` generation. Two test targets: the
  Unity C core tests + the QtTest webparse test, both under `ctest`.
- `.github/workflows/deploy.yml` — replace vcpkg/apt/brew steps with **`jurplel/install-qt-action@v3`**
  (`version: 5.15.2`, per-platform `arch` like the template); re-add the macOS `install_name_tool`
  Qt-rpath fix (from the template); the `release` job now stages just the four binaries + `package.ini`
  into `plugins/` (no `ts3websitepreview/` dep subdir) before zipping.
- `README.md` / `CLAUDE.md` — rewrite Build / dependencies / architecture: Qt-based, no native deps,
  no dynamic loading, no settings.rc, package = binaries only. Correct the (now different) test set.

## Networking design (note the trade-off)

Per fetch: build a `QNetworkRequest` (`FollowRedirectsAttribute`, UA header), `get()`, spin a local
`QEventLoop` until `finished`, read `readAll()` → `QByteArray` → `webparse`. QObjects are created
locally so this works on whatever thread `onTextMessageEvent` runs on. It still **blocks** that thread
during the fetch — same behaviour as today (already noted in CLAUDE.md), not a regression. The
in-client threading (dialog on the Qt thread, network on the callback thread) can only be confirmed in
a real TS3 client.

## Verification

- **Local (real):** `find_package(Qt5)` configure on Linux → build the plugin `.so` + both test EXEs;
  `ctest` runs the Unity core tests **and** the QtTest parser tests. Additionally, a throwaway local
  Qt CLI that fetches a real URL and prints the extracted title, to sanity-check fetch+parse
  end-to-end (Qt Network works here).
- **Cannot test here:** the MSVC and macOS builds, the `install-qt` CI, and in-TS3 runtime behaviour
  (Qt dialog thread, blocking network on the callback thread) — flagged for real testing; the first
  CI tag push will likely need touch-ups.
- Confirm the `.ts3_plugin` now contains only `package.ini` + `plugins/ts3websitepreview_*.{dll,so,dylib}`
  — no dependency subdir.
