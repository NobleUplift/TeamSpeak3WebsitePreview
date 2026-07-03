# Plan: Update Plugin to TS3 3.6.2 / API Version 26

## Context

The plugin was last built against the TeamSpeak 3 Plugin SDK from January 2016 (API version 22). TS3 3.6.2 (September 2023) requires API version 26. When the API version returned by `ts3plugin_apiVersion()` does not match what the client expects, TS3 silently refuses to load the plugin. Two callback signatures also changed between API 22 and API 26 (in `plugin.h` declarations only — neither is implemented, so they don't affect the compiled DLL today, but need correcting for future use). The SDK headers need refreshing so the `TS3Functions` struct definition the plugin compiles against matches what TS3 3.6.2 passes at runtime.

The user also wants the target TS3 version added to CLAUDE.md.

---

## Changes

### 1. `ts3websitepreview/plugin.c` — line 43
```c
// Before
#define PLUGIN_API_VERSION 22

// After
#define PLUGIN_API_VERSION 26
```

### 2. `ts3websitepreview/plugin.h` — two signature corrections

**Line 130** — `ts3plugin_onBanListEvent`: add `const char* mytsid` after `const char* uid` (added in API 23):
```c
// Before
PLUGINS_EXPORTDLL void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID,
    uint64 banid, const char* ip, const char* name, const char* uid,
    uint64 creationTime, ...);

// After
PLUGINS_EXPORTDLL void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID,
    uint64 banid, const char* ip, const char* name, const char* uid,
    const char* mytsid, uint64 creationTime, ...);
```

**Line 132** — `ts3plugin_onPluginCommandEvent`: add 3 invoker parameters at the end (added in API 23):
```c
// Before
PLUGINS_EXPORTDLL void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID,
    const char* pluginName, const char* pluginCommand);

// After
PLUGINS_EXPORTDLL void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID,
    const char* pluginName, const char* pluginCommand,
    anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity);
```

Note: Neither function is implemented in `plugin.c`, so only `plugin.h` needs editing.

### 3. SDK headers — fetch and overwrite from the official SDK repo

Source base URL: `https://raw.githubusercontent.com/teamspeak/ts3client-pluginsdk/master/include/`

Files to fetch and overwrite (relative to `ts3websitepreview/include/`):
- `ts3_functions.h`
- `plugin_definitions.h`
- `teamspeak/public_definitions.h`
- `teamspeak/public_errors.h`
- `teamspeak/public_errors_rare.h`
- `teamspeak/public_rare_definitions.h`
- `teamspeak/clientlib_publicdefinitions.h`
- `teamlog/logtypes.h`

These files are excluded from git by `.gitignore` (`include/*`), so the updates won't appear in the diff but are necessary for a correct build.

### 4. `CLAUDE.md`

Two edits:
- In the "Architecture" bullet for `ts3websitepreview`: update "API version 22" → "API version 26".
- Add a **Target Environment** section (after "What This Is") noting the tested TS3 client version and the required plugin API version.

New section:
```markdown
## Target Environment

- **TeamSpeak 3 Client**: 3.6.2 (released September 2023)
- **Plugin API version**: 26 (returned by `ts3plugin_apiVersion()`)

The plugin is Windows-only (Win32 and x64 DLL). It has no dependency on Qt or other TS3-internal libraries.
```

---

## Verification

1. Build `ts3websitepreview` for Win32 and x64 via MSBuild or the IDE — both should compile cleanly.
2. Copy (or package) the resulting DLL into the TS3 plugins directory.
3. Launch TS3 3.6.2 → Settings → Plugins → confirm "Website Preview" appears and loads without an "incompatible API version" error.
4. In a channel chat, send a message that is exactly `[URL]https://www.example.com[/URL]` — confirm it is replaced with `"Example Domain" <[URL]https://www.example.com[/URL]>`.
