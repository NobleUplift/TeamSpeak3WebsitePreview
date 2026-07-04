# TeamSpeak 3 Website Preview Plugin

A TeamSpeak 3 client plugin that intercepts outbound chat messages containing bare URL tags and automatically prepends the webpage title. When you send `[URL]https://example.com[/URL]`, the plugin fetches the page, extracts the `<title>` element, and resends the message as `"Example Domain" <[URL]https://example.com[/URL]>`.

**Started**: Thursday, December 22, 2011, 8:08:39 AM  
**First working build**: April 24, 2016  
**Broken by TS3 API updates, abandoned**: March 25, 2017  
**Finally working again**: July 3, 2026  

This project took approximately **14.5 years** from inception to a stable, correctly packaged, working state. The rest of this document is a forensic account of every reason why.

---

## Why This Plugin Did Not Work For Over A Decade

This is written as a reference for anyone (including future me) returning to C plugin development on Windows. Every single one of these issues independently caused a silent failure with no useful error message. Several of them were present simultaneously.

---

### 1. Dependency DLLs Must Go in a Subdirectory, But Windows Won't Search There Automatically

**The rule**: The conventional TS3 plugin package layout puts dependency DLLs in `%AppData%\TS3Client\plugins\<pluginname>\` — a subdirectory named after the plugin (without the `_win64` suffix). However, the Windows DLL loader does **not** automatically search subdirectories when resolving imports. The standard DLL search order is: (1) the directory containing the loaded DLL, (2) system directories, (3) the Windows directory, (4) the current directory, (5) directories in PATH. A subdirectory like `plugins\ts3websitepreview\` appears in none of these.

**What happened**: Putting `libcurl.dll` in `plugins\ts3websitepreview\` caused the plugin to fail to load immediately, because Windows couldn't find it when resolving the plugin DLL's imports at load time. Putting it in `plugins\` alongside the plugin DLL made it findable, but this is wrong per convention and pollutes the shared plugins directory.

**The deeper constraint**: Making the subdirectory work requires either (a) delay-loading the dependency and manually loading it at runtime via `LoadLibraryExW` with a full absolute path, or (b) adding the subdirectory to the DLL search path globally via `SetDllDirectory` before the dependency is first needed.

**Fix**: Load `libcurl.dll` and `libxml2.dll` entirely at runtime using `LoadLibraryExW` with the full path constructed from the plugin DLL's own location (obtained via `GetModuleFileNameW`). This is done in `ts3plugin_init()` before any functions from those libraries are called.

---

### 2. `libxml2.dll` Cannot Be Delay-Loaded Due to a Data Symbol Export

**The rule**: Windows delay-loading works by replacing each imported function with a stub that loads the DLL and resolves the real address on first call. This mechanism only works for **function** imports. It cannot handle **data symbol** imports — imported global variables.

**What happened**: libxml2 exports `xmlFree` not as a function but as a global variable of type `xmlFreeFunc` (itself a function pointer: `void (*)(void*)`):

```c
XMLPUBVAR xmlFreeFunc xmlFree;  // expands to: extern __declspec(dllimport) xmlFreeFunc xmlFree;
```

When our code calls `xmlFree(ptr)`, the compiler generates code that reads the value of the imported variable `__imp__xmlFree` (the function pointer stored in libxml2's data segment) and calls through it. The MSVC linker detects this data symbol import and refuses to delay-load the DLL:

```
fatal error LNK1194: cannot delay-load 'libxml2.dll' due to import of data symbol '__imp__xmlFree'
```

This error is unambiguous but the solution isn't obvious. The git history shows multiple failed attempts at this exact problem (`3e94c9a "Various attempts at fixing xmlFree memory leak and delay-loading"`). The conclusion at the time appears to have been to comment out the delay-loading code and give up.

**Fix**: Do not use delay-loading for libxml2 at all. Instead, load it manually via `LoadLibraryExW` and obtain all needed function pointers via `GetProcAddress`. Every direct call to a libxml2 function in `plugin.c` must be replaced with a call through a function pointer variable.

---

### 3. `xmlFree` via `GetProcAddress` Requires a Double Dereference

**The rule**: `GetProcAddress(hModule, "symbol")` returns the address **of** the named symbol in the loaded module. For a function, that address is directly callable. For a data symbol (a global variable), that address is a pointer **to** the variable — you must dereference it to get the variable's value.

**What happened**: `xmlFree` is a global variable whose value is a function pointer. `GetProcAddress(hLibxml2, "xmlFree")` returns the address of the `xmlFree` variable inside libxml2's data segment — not the function pointer value stored in that variable. Calling the `GetProcAddress` return value directly as a function would jump to libxml2's data segment and execute whatever bytes are there as code, immediately crashing.

**Fix**:
```c
typedef void (*pfnXmlFree_t)(void*);
pfnXmlFree_t* pVar = (pfnXmlFree_t*)GetProcAddress(hLibxml2, "xmlFree");
pfn_xmlFree = pVar ? *pVar : NULL;  // dereference: pVar is &xmlFree, *pVar is xmlFree's value
```

The cast to `pfnXmlFree_t*` interprets the `GetProcAddress` result as a pointer-to-function-pointer. Dereferencing it yields the actual function pointer stored in libxml2's global `xmlFree` variable.

---

### 4. `libiconv.dll` Was Named `iconv.dll`

**The rule**: Windows DLL resolution is by filename. When libxml2.dll has an import table entry for `libiconv.dll`, Windows searches for a file named exactly `libiconv.dll`. A file named `iconv.dll` in the same directory does not satisfy this import, regardless of its content.

**What happened**: The pre-built dependency DLL in the repository was named `iconv.dll`. libxml2.dll imports `libiconv.dll`. Windows could not resolve the import, causing libxml2 to fail to load, which caused the plugin to fail to load, which produced a generic unhelpful error.

**Why it was hard to diagnose**: The plugin load failure message from TS3 didn't mention libxml2 or libiconv by name. `dumpbin /dependents libxml2.dll` reveals the exact import name (`libiconv.dll`), but checking DLL import tables is not an obvious first debugging step.

**Fix**: The solution project includes a `libiconv` project that compiles iconv from source. Because the project is named `libiconv`, MSBuild's default `TargetName` is `libiconv`, and the output is `libiconv.dll`.

---

### 5. Loading libxml2 From a Subdirectory Requires `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`

**The rule**: When you call `LoadLibraryExW(fullPathToSomeDll, NULL, 0)`, Windows loads that DLL from the specified path. However, when it resolves *that DLL's* own imports (e.g., libxml2 needing libiconv), it uses the standard search order — which does **not** include the directory the DLL was just loaded from unless you tell it to.

**What happened**: Loading libxml2.dll from `plugins\ts3websitepreview\libxml2.dll` worked (the file was found by the full path). But libxml2 then tried to load libiconv.dll using the standard search order. Since `plugins\ts3websitepreview\` is not in the standard search order and libiconv.dll was not in any of the searched locations, the load failed.

**Fix**: Use `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` flags:

```c
hLibxml2 = LoadLibraryExW(fullPath, NULL,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
```

`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` instructs Windows to search the directory containing the DLL being loaded when resolving that DLL's own imports — finding `libiconv.dll` in `plugins\ts3websitepreview\` alongside `libxml2.dll`. `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` ensures system DLLs (`KERNEL32.dll`, `MSVCR100.dll`, `WSOCK32.dll`) are still found via their normal locations.

---

### 6. libcurl Was Linked Against OpenSSL, Which TS3 Also Bundles

**The rule**: When two DLLs in the same process both load different copies of OpenSSL — one bundled with the application, one bundled with a plugin dependency — there can be conflicts at initialization, symbol resolution, or runtime state sharing between OpenSSL instances.

**What happened**: The pre-built `libcurl.dll` was linked against OpenSSL and expected `libssl-1_1-x64.dll` and `libcrypto-1_1-x64.dll` to be present. TS3 3.6.x bundles OpenSSL 1.1.1u internally. Placing the right version of these DLLs alongside the plugin was fragile — version 1.1.1u was removed from the common distribution channels (e.g., Win32OpenSSL only carries 3.x and 4.x as of 2026), making it impossible to legally source a matching binary.

**Fix**: Build libcurl from source using the Windows SChannel backend (`-DCURL_USE_SCHANNEL=ON` in cmake). SChannel is Windows' built-in TLS implementation (the same one used by Internet Explorer, Edge, and most native Windows networking). The resulting `libcurl.dll` has zero OpenSSL dependency — its only TLS-related imports are `bcrypt.dll`, `crypt32.dll`, and `secur32.dll`, all of which are system DLLs present on every Windows installation.

---

### 7. The VS 2010 Build Toolset (v100) No Longer Exists

**The rule**: MSBuild selects a compiler toolchain via the `<PlatformToolset>` property. The project was created with Visual Studio 2010 (toolset `v100`). Visual Studio 2022 does not ship the v100 toolset and will refuse to build with error MSB8020.

**What happened**: Attempting to build with VS 2022 without overriding the toolset failed immediately. The vcxproj files did not specify a toolset (defaulting to v100 from the solution format version), and VS 2022 rejected this.

**Fix**: Add `<PlatformToolset>v143</PlatformToolset>` to every `<PropertyGroup Label="Configuration">` block in every `.vcxproj` in the solution. `v143` is the VS 2022 toolset. Alternatively, pass `/p:PlatformToolset=v143` to MSBuild on the command line (as in `build_plugin.bat`), but the vcxproj-level setting is more durable.

---

### 8. Release|x64 Was Pointing to the Win32 Library Directory

**The rule**: 64-bit import libraries (`.lib` files that describe 64-bit DLL exports) are not interchangeable with 32-bit ones. Linking a 64-bit object against a 32-bit import library produces unresolved external symbol errors.

**What happened**: The `Release|x64` configuration in `ts3websitepreview.vcxproj` had:
```xml
<AdditionalLibraryDirectories>.\lib</AdditionalLibraryDirectories>
<AdditionalDependencies>lib\libcurl.lib;lib\libxml2.lib;lib\iconv.lib;...</AdditionalDependencies>
```

The x64 libraries live in `.\lib64\`, not `.\lib\`. This caused linker failures for x64 Release builds that were distinct from the Win32 linker failures, making it appear the project could never produce a working x64 build.

**Fix**: Change to `.\lib64` and `lib64\libcurl.lib` etc. for the `Release|x64` configuration.

---

### 9. Win32 and x64 Release Builds Overwrote Each Other

**The rule**: MSBuild uses `<OutDir>` to determine where the linker places the output DLL. If two configurations share the same `<OutDir>`, building one will overwrite the other's output.

**What happened**: Both `Release|Win32` and `Release|x64` had `<OutDir>..\ts3websitepreview\</OutDir>`. Building x64 would place `ts3websitepreview.dll` (later `ts3websitepreview_win64.dll`) in the same directory as the Win32 build. Only the most recently built architecture would be present.

**Fix**: Route each to its own `.ts3_plugin` package directory:
- Win32: `..\ts3websitepreview_win32.ts3_plugin\plugins\`
- x64: `..\ts3websitepreview_win64.ts3_plugin\plugins\`

---

### 10. `clientlib_publicdefinitions.h` Was Removed from the SDK in API 23

**The rule**: SDK header files are versioned along with the API. When the SDK removes a header, code that includes it fails to compile.

**What happened**: Between API 22 and API 23, the TS3 SDK removed `clientlib_publicdefinitions.h`. The `Visibility` and `ConnectStatus` enums it defined were moved into `public_definitions.h`. The plugin included the old header, which no longer existed in any downloadable SDK package, causing a compilation error on fresh setups.

**Fix**: Replace the file with a one-line compatibility shim:
```c
#ifndef CLIENTLIB_PUBLICDEFINITIONS_H
#define CLIENTLIB_PUBLICDEFINITIONS_H
#include "public_definitions.h"
#endif
```

---

### 11. Two Callback Function Signatures Changed in API 23

**The rule**: The TS3 plugin API is defined by a set of function signatures the plugin exports. If TS3 calls a plugin-exported function with a different signature than the one the plugin was compiled with, the arguments will be misread from the stack (x86) or registers (x64), and if the function returns a value, the return type mismatch may cause additional corruption. On x64, calling a function that expects 3 parameters with 6 arguments passed in registers simply means the extra register arguments are ignored — but the plugin's return value and any out-parameters may still be wrong.

**What happened**: Two declarations in `plugin.h` had stale signatures:

`ts3plugin_onBanListEvent` — gained `const char* mytsid` after `const char* uid` (API 23):
```c
// old (API 22)
void ts3plugin_onBanListEvent(uint64, uint64, const char*, const char*, const char*, uint64, ...);
// new (API 26)
void ts3plugin_onBanListEvent(uint64, uint64, const char*, const char*, const char*, const char*, uint64, ...);
```

`ts3plugin_onPluginCommandEvent` — gained three invoker parameters (API 23):
```c
// old
void ts3plugin_onPluginCommandEvent(uint64, const char*, const char*);
// new
void ts3plugin_onPluginCommandEvent(uint64, const char*, const char*, anyID, const char*, const char*);
```

Neither function had an implementation in `plugin.c`, so these were declaration-only mismatches. TS3 would call these with the new ABI and the plugin's exported symbol would have the old ABI, producing silent misbehavior if they were ever invoked (e.g., ban list events or inter-plugin commands).

**Fix**: Update the declarations in `plugin.h` to match the API 26 signatures.

---

### 12. The Original Delay-Load Code Used the Wrong SEH Exception Filter

**The detail**: The original commented-out `__try/__except` blocks used `FACILITY_VISUALCPP` as the exception filter expression:

```c
__try {
    if (FAILED(__HrLoadAllImportsForDll("libcurl.dll"))) { ... }
} __except(FACILITY_VISUALCPP) {  // FACILITY_VISUALCPP == 0x60 == 96
    ...
}
```

In Windows SEH, `__except(expr)` expects one of three values: `EXCEPTION_EXECUTE_HANDLER` (1), `EXCEPTION_CONTINUE_SEARCH` (0), or `EXCEPTION_CONTINUE_EXECUTION` (-1). In practice, the MSVC runtime treats any positive non-zero value as `EXCEPTION_EXECUTE_HANDLER`, so the value 96 accidentally worked. The correct constant is `EXCEPTION_EXECUTE_HANDLER`.

This was not the root cause of any failure but represents the kind of subtly wrong code that accumulates when debugging under time pressure without a reference.

---

### 13. TS3's Error Messages Were Essentially Useless

**The summary**: Every single failure described above produced one of the following messages in TS3's plugin manager:

- *(plugin doesn't appear at all)*
- `Failed to open plugin.: 0` — LoadLibrary returned NULL, GetLastError returned 0 (or the DLL loaded but returned a failure from ts3plugin_init)
- `Failed to open plugin.: -1127266411` — LoadLibrary returned NULL with a specific Win32 error encoded as a signed HRESULT

None of these messages identified which DLL was missing, what the actual Windows error code was, which API version was mismatched, or what function had a wrong signature. The only reliable diagnostic tool was `dumpbin /dependents` to inspect DLL import tables and verify the DLL dependency chain by hand.

---

## What Finally Fixed It (July 3, 2026)

All thirteen issues above were present simultaneously or sequentially. The final working state required:

1. Build libcurl from source with SChannel backend (no OpenSSL)
2. Add `<PlatformToolset>v143</PlatformToolset>` to all project configurations
3. Fix `Release|x64` to use `lib64\` not `lib\`
4. Route each Release config to its own `.ts3_plugin` output directory
5. Replace the `clientlib_publicdefinitions.h` include with a shim
6. Fix the two changed callback declarations in `plugin.h`
7. Replace all direct `libcurl` and `libxml2` function calls in `plugin.c` with `LoadLibraryExW` / `GetProcAddress` dynamic loading, so all dependency DLLs can live in `plugins\ts3websitepreview\` per TS3 convention
8. Handle the `xmlFree` data symbol double-dereference correctly
9. Use `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` so libxml2 can find libiconv alongside it
10. Rename/rebuild `iconv.dll` to `libiconv.dll`

---

## Installation

Copy to `%AppData%\TS3Client\plugins\`:

```
ts3websitepreview_win64.dll          (or _win32.dll for 32-bit TS3)
ts3websitepreview\
    libcurl.dll
    libxml2.dll
    libiconv.dll
    zlib1.dll                        (Win32 only; x64 libxml2 doesn't need it)
```

---

## Building

Requires Visual Studio 2022 with the Desktop C++ workload. The binary DLLs (`libcurl.dll`, `libxml2.dll`, `libiconv.dll`) and their import libraries are not in the repository.

Use `build_plugin.bat` or call MSBuild directly:

```bat
msbuild ts3websitepreview\ts3websitepreview.vcxproj ^
    /p:Configuration=Release /p:Platform=x64 ^
    /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0
```

See `CLAUDE.md` for full build and dependency instructions.
