# Plan: Complete Build Process — Package to .ts3_plugin ZIP

## Context

The build script currently only compiles the plugin DLL; packaging (copying dependency DLLs, creating package.ini, zipping into a .ts3_plugin archive) is done manually. The two `ts3websitepreview_win32.ts3_plugin\` and `ts3websitepreview_win64.ts3_plugin\` root directories served as staging areas whose name matched the final product name, which caused confusion. The goal is a fully automated build that emits `Release\ts3websitepreview_win32.ts3_plugin` and `Release\ts3websitepreview_win64.ts3_plugin` as installable ZIP archives with `plugins\` and `package.ini` at their root (no wrapper folder), using a single source `package.ini` template.

---

## zlib1.dll Decision

**Delete it — do not include in Win32 or x64 packages.**

`zlib1.dll` was placed in the Win32 staging directory ~15 years ago as a dev tool for creating ZIP archives, not as a runtime dependency of the plugin or its vendor DLLs. It is not needed in the packaged output.

---

## New Directory Layout

```
Release\
  staging_win32\              ← temporary staging (created by build, can be deleted after)
    plugins\
      ts3websitepreview_win32.dll   ← compiled by MSBuild
      ts3websitepreview\
        libiconv.dll               ← compiled by MSBuild (libiconv project)
        libcurl.dll                ← copied from ts3websitepreview\lib\
        libxml2.dll                ← copied from ts3websitepreview\lib\
    package.ini                    ← generated from template
  staging_x64\               ← same layout for x64
    plugins\
      ts3websitepreview_win64.dll
      ts3websitepreview\
        libiconv.dll
        libcurl.dll
        libxml2.dll
    package.ini
  ts3websitepreview_win32.ts3_plugin   ← final ZIP output
  ts3websitepreview_win64.ts3_plugin   ← final ZIP output

ts3websitepreview\
  package.ini                ← NEW: generic template (replaces the two per-arch copies)
```

---

## Files to Create / Modify

### 1. Create `ts3websitepreview\package.ini` (template)

Move from the two per-arch copies into one generic file. The only difference between them was the `Platforms` line. Use a `{PLATFORM}` placeholder that the build script substitutes:

```ini
Name = Website Preview
Type = Plugin
Author = NobleUplift (Patrick Seiter)
Version = 1.0
Platforms = {PLATFORM}
Description = "This plugin parses URLs in channel chat and appends a preview of the webpage along with its title and description to the end of the chat message."
```

### 2. Modify `ts3websitepreview\ts3websitepreview.vcxproj`

Change the **Release** `OutDir` (only Release — leave Debug alone):

| Config | Old OutDir | New OutDir |
|---|---|---|
| Release\|Win32 | `$(SolutionDir)ts3websitepreview_win32.ts3_plugin\plugins\` | `$(SolutionDir)Release\staging_win32\plugins\` |
| Release\|x64 | `$(SolutionDir)ts3websitepreview_win64.ts3_plugin\plugins\` | `$(SolutionDir)Release\staging_x64\plugins\` |

### 3. Modify `libiconv\libiconv.vcxproj`

Change the **Release** `OutDir`:

| Config | Old OutDir | New OutDir |
|---|---|---|
| Release\|Win32 | `$(SolutionDir)ts3websitepreview_win32.ts3_plugin\plugins\ts3websitepreview\` | `$(SolutionDir)Release\staging_win32\plugins\ts3websitepreview\` |
| Release\|x64 | `$(SolutionDir)ts3websitepreview_win64.ts3_plugin\plugins\ts3websitepreview\` | `$(SolutionDir)Release\staging_x64\plugins\ts3websitepreview\` |

### 4. Rewrite `build_plugin.bat`

Full replacement. Key additions vs. today's script:
- Builds `libiconv.vcxproj` explicitly for both platforms before the plugin.
- Cleans staging directories before each build so stale files don't accumulate.
- Copies vendor DLLs (`libcurl.dll`, `libxml2.dll`) from `ts3websitepreview\lib\` / `ts3websitepreview\lib64\` into the staging subdirectory.
- Generates `package.ini` via PowerShell string replacement (`{PLATFORM}` → `win32` or `win64`).
- Deletes any prior `.ts3_plugin` output file, then creates a fresh ZIP with PowerShell `Compress-Archive -Path staging\* -DestinationPath Release\ts3websitepreview_winXX.ts3_plugin`. Using `-Path staging\*` (glob, not the directory itself) ensures the ZIP root contains `plugins\` and `package.ini` with no wrapper folder.

### 5. Delete old root-level staging directories

- `ts3websitepreview_win32.ts3_plugin\` (entire directory, including `zlib1.dll`)
- `ts3websitepreview_win64.ts3_plugin\` (entire directory)

---

## Verification

1. Run `build_plugin.bat` from project root.
2. Confirm no MSBuild errors for libiconv (Win32 + x64) and ts3websitepreview (Win32 + x64).
3. Confirm `Release\staging_win32\plugins\` contains `ts3websitepreview_win32.dll`.
4. Confirm `Release\staging_win32\plugins\ts3websitepreview\` contains `libcurl.dll`, `libxml2.dll`, `libiconv.dll`.
5. Confirm `Release\staging_x64\plugins\ts3websitepreview\` contains `libcurl.dll`, `libxml2.dll`, `libiconv.dll`.
6. Confirm `Release\ts3websitepreview_win32.ts3_plugin` and `Release\ts3websitepreview_win64.ts3_plugin` exist and are valid ZIPs.
7. Open each ZIP (rename to `.zip` if needed, or use 7-Zip) and verify the root contains exactly `plugins\` and `package.ini` — no wrapper folder.
8. Check `package.ini` in each ZIP has the correct `Platforms = win32` / `Platforms = win64` line.
9. Optionally install one of the `.ts3_plugin` files in TS3 to confirm the plugin loads.
