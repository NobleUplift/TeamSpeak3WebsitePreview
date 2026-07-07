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

Download the `.ts3_plugin` package for your architecture and double-click it while TeamSpeak 3 is **closed**.

> **Note:** Installing while TS3 is running shows a misleading "Fail to install Add-On. Do you want to retry as Administrator?" prompt — running as Administrator does not help. Close TS3 first, then install.

Manual layout (if installing by hand into `%AppData%\TS3Client\plugins\`):

```
plugins\
  ts3websitepreview_win64.dll       ← main plugin DLL (_win32.dll for 32-bit TS3)
  ts3websitepreview\
    libcurl.dll
    libxml2.dll
    libiconv.dll
    zlib1.dll                       ← Win32 only
```

---

## Requirements

- **TeamSpeak 3** 3.6.2 or later (Plugin API 26)
- **Windows** (Win32 or x64) — the plugin has no macOS or Linux build

---

## Building

Requires **Visual Studio 2022** (Desktop C++ workload) and **CMake ≥ 3.21**. The build is
CMake-based — there is no `.sln`. Two prerequisites must be in place first:

1. **The TS3 SDK submodule** — clone recursively, or initialise it in an existing clone:
   ```bat
   git clone --recursive <repo-url>
   :: or, in an existing clone:
   git submodule update --init
   ```
2. **The vendored dependencies** — the `libcurl`, `libxml2`, and `iconv` headers, import libs, and
   DLLs are not in the repository. Place them under `third_party/` as described in
   [third_party/README.md](third_party/README.md). libcurl must use the **SChannel** backend (no OpenSSL).

Build both architectures with the bundled presets (the VS generator is single-arch per build tree,
so each arch gets its own configure + build):

```bat
cmake --preset win32 && cmake --build --preset win32
cmake --preset win64 && cmake --build --preset win64
```

Output: `build\win32\out\ts3websitepreview_win32.dll` and `build\win64\out\ts3websitepreview_win64.dll`.

> Assembling the DLL + dependency DLLs into the installable `.ts3_plugin` package is **not** done by
> the local build — that step is left to CI (a GitHub Actions workflow, to be added). Until then,
> package by hand following the layout in [CLAUDE.md](CLAUDE.md#ts3-plugin-package-structure).

To also build and run the 23 unit tests, configure with `-DBUILD_TESTS=ON`:

```bat
cmake -S . -B build\win64 -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=ON
cmake --build build\win64 --config Release
ctest --test-dir build\win64 -C Release
```

Plugin metadata (name, version, author, description) lives in the `project()`/`PLUGIN_*` block at
the top of `CMakeLists.txt`. See `CLAUDE.md` for full build, dependency, and packaging details.

---

## History

This plugin has an unusually long development history — started in 2011, first working build in 2016, broken by API changes, abandoned, and finally revived and correctly packaged in 2026. See [HISTORY.md](HISTORY.md) for a detailed forensic account of every reason it didn't work for over a decade.
