# third_party/

Vendored native dependencies for the plugin: **libcurl**, **libxml2**, and **iconv**.
These are **not** checked into git (see `.gitignore`) and must be supplied externally —
a fresh clone will not build until you populate this directory.

The plugin loads `libcurl.dll` and `libxml2.dll` at runtime via `LoadLibraryExW` +
`GetProcAddress`, so the **plugin DLL links none of these**; it only needs their headers to
compile. The unit tests (`-DBUILD_TESTS=ON`) *do* call libxml2 directly and therefore link the
import `.lib`s.

## Expected layout

```
third_party/
  include/
    curl/          curl.h + the rest of the libcurl headers
    libxml/        HTMLparser.h, xpath.h, globals.h + the rest of the libxml2 headers
  lib/             Win32 (x86) binaries:
    libcurl.dll  libxml2.dll  iconv.dll        (runtime, shipped in the package)
    libcurl.lib  libxml2.lib  iconv.lib  zlib1.lib   (import libs, tests only)
  lib64/           x64 binaries:
    libcurl.dll  libxml2.dll  iconv.dll        (runtime, shipped in the package)
    libcurl.lib  libxml2.lib  iconv.lib        (import libs, tests only)
```

This mirrors the old `ts3websitepreview/{include,lib,lib64}` vendor dirs — copy those files
here unchanged.

## Notes

- `iconv.dll` is shipped **renamed to `libiconv.dll`** inside the `.ts3_plugin` package, because
  `libxml2.dll` imports it by that exact name. The build does the rename automatically.
- libxml2 must be a version that still ships `libxml/globals.h` (removed in libxml2 ≥ 2.12).
- These must be **SChannel-backed** libcurl builds (Windows-native TLS, no OpenSSL), matching the
  target TeamSpeak 3.6.2 client environment.
