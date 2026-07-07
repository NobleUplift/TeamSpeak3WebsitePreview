# TeamSpeak 3 Website Preview Plugin

A TeamSpeak 3 client plugin that fetches the title of URLs you send in chat and presents them inline — so your channel sees context, not a bare link.

---

## How It Works

The plugin intercepts your outbound chat messages before they are displayed and looks for URLs wrapped in TS3's `[URL]...[/URL]` tags (which TS3 applies automatically when you type or paste a URL). It fetches each page, extracts its title, and resends a formatted version of your message.

There are two modes of operation.

---

### Use Case 1 — URL-only message

When your entire message is a single URL and nothing else:

**You type:**
```
https://en.wikipedia.org/wiki/TeamSpeak
```

**Channel sees:**
```
"TeamSpeak - Wikipedia" <https://en.wikipedia.org/wiki/TeamSpeak>
(optional) TeamSpeak is a proprietary voice-over-Internet Protocol application...
```

The page title is shown first, followed by the clickable link. If the page provides an `og:description` meta tag and the **Show page description in chat** setting is enabled, the description is appended on a second line.

---

### Use Case 2 — URL embedded in a typed message

When you write a message that contains a URL alongside other text:

**You type:**
```
Check out what I found: https://example.com/article
```

**Channel sees:**
```
Check out what I found: "Example Article Title" <https://example.com/article>
```

Each URL in the message is replaced with its titled form. If a message contains multiple URLs, each is fetched and titled independently. `og:description` is never used in this mode — only the page title is inserted.

This mode is gated by the **Show title inline in typed messages** setting.

---

## Settings

Open the plugin settings from **Plugins → Website Preview → Configure**.

| Setting | Default | Description |
|---------|---------|-------------|
| **Show page description in chat** | On | Appends the page's `og:description` on a second line for URL-only messages (use case 1). Has no effect on use case 2. |
| **Show title inline in typed messages** | On | Enables use case 2. When off, messages containing URLs alongside text are sent unchanged. |

Settings are saved to `%AppData%\TS3Client\config\plugins\ts3websitepreview.ini`.

---

## Installation

Download the `.ts3_plugin` package and double-click it while TeamSpeak 3 is **closed**. One package
covers all platforms — TS3 loads the binary matching the client's OS/arch.

> **Note:** Installing while TS3 is running shows a misleading "Fail to install Add-On. Do you want to retry as Administrator?" prompt — running as Administrator does not help. Close TS3 first, then install.

Package layout (what the `.ts3_plugin` ZIP contains):

```
package.ini
plugins/
  ts3websitepreview_win64.dll        ← Windows binaries (name suffix per arch)
  ts3websitepreview_win32.dll
  ts3websitepreview_linux_amd64.so   ← Linux / macOS binaries
  ts3websitepreview_mac.dylib
  ts3websitepreview/                 ← Windows-only bundled deps, in arch subdirs
    win64/  (libcurl.dll, libxml2.dll, + their runtime DLLs)
    win32/  (libcurl.dll, libxml2.dll, + their runtime DLLs)
```

On Windows the plugin loads its bundled `libcurl`/`libxml2` at runtime from the arch subdir. On
Linux/macOS it links the system libcurl + libxml2, so no dependency files are bundled.

---

## Requirements

- **TeamSpeak 3** 3.6.2 or later (Plugin API 26)
- **Windows** (Win32 / x64), **Linux** (x86-64), or **macOS**
- The **settings dialog is Windows-only** (native Win32). On Linux/macOS the plugin runs with its
  saved settings and no in-client configure button; edit `ts3websitepreview.ini` in the plugin's
  config dir to change them.

---

## Building

The build is **CMake-based** (CMake ≥ 3.21) — there is no `.sln`. First get the **TS3 SDK submodule**
(both `libcurl` and `libxml2` are needed on every platform):

```sh
git clone --recursive <repo-url>
# or, in an existing clone:
git submodule update --init
```

**Windows** — Visual Studio 2022 (Desktop C++ workload). Supply `libcurl` + `libxml2` + `iconv`
(headers, import libs, DLLs) under `third_party/` as described in
[third_party/README.md](third_party/README.md); libcurl must use the **SChannel** backend (no OpenSSL).
The VS generator is single-arch per build tree, so build each arch separately:

```bat
cmake --preset win32 && cmake --build --preset win32
cmake --preset win64 && cmake --build --preset win64
```

**Linux** — install system deps, then build:

```sh
sudo apt-get install -y libcurl4-openssl-dev libxml2-dev   # Debian/Ubuntu
cmake --preset linux && cmake --build --preset linux
```

**macOS** — `brew install libxml2` (libcurl is system), then `cmake --preset mac && cmake --build --preset mac`.

Output binary: `build/<preset>/out/ts3websitepreview_<suffix>{.dll,.so,.dylib}`. Assembling all
platforms into one installable `.ts3_plugin` is done by CI
([.github/workflows/deploy.yml](.github/workflows/deploy.yml)) on a `v*` tag push.

To also build and run the 32 unit tests, add `-DBUILD_TESTS=ON` and run via CTest — e.g. on Linux:

```sh
cmake --preset linux -DBUILD_TESTS=ON && cmake --build --preset linux
ctest --test-dir build/linux --output-on-failure
```

Plugin metadata (name, version, author, description) lives in the `project()`/`PLUGIN_*` block at
the top of `CMakeLists.txt`. See `CLAUDE.md` for full build, dependency, and packaging details.

---

## History

This plugin has an unusually long development history — started in 2011, first working build in 2016, broken by API changes, abandoned, and finally revived and correctly packaged in 2026. See [HISTORY.md](HISTORY.md) for a detailed forensic account of every reason it didn't work for over a decade.
