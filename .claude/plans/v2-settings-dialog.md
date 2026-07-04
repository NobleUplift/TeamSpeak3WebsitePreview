# Plan: v2.0 Plugin Settings UI

## Context

Add a settings dialog to the plugin, accessible via the "Settings" button in the TS3 plugin list (the same entry point used by the ClientQuery plugin). The first setting is a checkbox to enable or disable printing the og:description in chat. Settings persist across restarts via an INI file stored next to the plugin DLL.

The plugin is pure C with no Qt dependency, so the dialog is a native Win32 `DIALOGEX` resource shown via `DialogBox()`. The `qParentWidget` parameter of `ts3plugin_configure` is a Qt pointer we cannot use from C — `NULL` is passed as the HWND parent, which produces a standalone modal dialog.

---

## Files to Create

| Path | Purpose |
|---|---|
| `ts3websitepreview/resource.h` | `#define` IDs for dialog and controls |
| `ts3websitepreview/settings.rc` | Win32 `DIALOGEX` resource definition |
| `ts3websitepreview/settings.h` | `PluginSettings` struct + `Settings_Load`/`Settings_Save` declarations |
| `ts3websitepreview/settings.c` | INI read/write using `GetPrivateProfileInt` / `WritePrivateProfileStringA` |

## Files to Modify

| Path | Change |
|---|---|
| `ts3websitepreview/plugin.c` | Implement `ts3plugin_offersConfigure`, `ts3plugin_configure`, store plugin path, gate og_desc on setting |
| `ts3websitepreview/ts3websitepreview.vcxproj` | Add `settings.c`, `settings.h`, `settings.rc`, `resource.h`; add `<ResourceCompile>` item |
| `ts3websitepreview/ts3websitepreview.vcxproj.filters` | Categorise new files |

---

## Step 1 — resource.h

```c
#define IDD_SETTINGS          101
#define IDC_CHECK_DESCRIPTION 201
```

---

## Step 2 — settings.rc

```rc
#include "resource.h"

IDD_SETTINGS DIALOGEX 0, 0, 220, 70
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "TS3 Website Preview Settings"
FONT 8, "MS Shell Dlg", 0, 0, 0x1
BEGIN
    AUTOCHECKBOX "Show page description in chat", IDC_CHECK_DESCRIPTION, 10, 12, 200, 10
    DEFPUSHBUTTON "OK",     IDOK,     110, 46, 45, 14
    PUSHBUTTON    "Cancel", IDCANCEL, 160, 46, 45, 14
END
```

---

## Step 3 — settings.h / settings.c

**settings.h:**
```c
typedef struct { int show_description; } PluginSettings;
extern PluginSettings g_settings;
void Settings_Load(const char* plugin_path);
void Settings_Save(const char* plugin_path);
```

**settings.c** — INI file at `<plugin_path>ts3websitepreview.ini` (the `getPluginPath` result already ends with `\`):
```c
static void GetIniPath(const char* plugin_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%sts3websitepreview.ini", plugin_path);
}

void Settings_Load(const char* plugin_path) {
    char ini[MAX_PATH];
    GetIniPath(plugin_path, ini, sizeof(ini));
    g_settings.show_description = GetPrivateProfileIntA("Settings", "ShowDescription", 1, ini);
}

void Settings_Save(const char* plugin_path) {
    char ini[MAX_PATH];
    char val[4];
    GetIniPath(plugin_path, ini, sizeof(ini));
    snprintf(val, sizeof(val), "%d", g_settings.show_description);
    WritePrivateProfileStringA("Settings", "ShowDescription", val, ini);
}
```

Default for `ShowDescription` is `1` (enabled).

---

## Step 4 — plugin.c changes

**New static variable** — store plugin path for use by `ts3plugin_configure`:
```c
static char g_pluginPath[MAX_PATH] = "";
```

**In `ts3plugin_init()`** — after the existing `getPluginPath` call, copy to global and load settings:
```c
ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);
strncpy_s(g_pluginPath, sizeof(g_pluginPath), pluginPath, _TRUNCATE);
Settings_Load(g_pluginPath);
```

**`ts3plugin_offersConfigure()`** — currently returns 0; change to return 1 (Qt thread, shows Settings button):
```c
int ts3plugin_offersConfigure() { return 1; }
```

**`ts3plugin_configure()`** — implement dialog:
```c
INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            CheckDlgButton(hwnd, IDC_CHECK_DESCRIPTION,
                g_settings.show_description ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                g_settings.show_description =
                    (IsDlgButtonChecked(hwnd, IDC_CHECK_DESCRIPTION) == BST_CHECKED) ? 1 : 0;
                Settings_Save(g_pluginPath);
                EndDialog(hwnd, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
            }
            return TRUE;
    }
    return FALSE;
}

void ts3plugin_configure(void* handle, void* qParentWidget) {
    (void)handle; (void)qParentWidget;
    HMODULE hDll = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(void*)ts3plugin_configure, &hDll);
    DialogBox(hDll, MAKEINTRESOURCE(IDD_SETTINGS), NULL, SettingsDlgProc);
}
```

Note: `GetModuleHandleExW` with `GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS` is used instead of `GetModuleHandleW(NULL)` — the latter returns the host EXE (TS3 client), not the plugin DLL. The resource `IDD_SETTINGS` lives in the DLL, so we need the DLL's own HMODULE.

**In `ts3plugin_onTextMessageEvent()`** — gate og_desc before passing to `BuildPreviewMessage`:
```c
BuildPreviewMessage(title, url,
    g_settings.show_description ? og_desc : NULL,
    og_image, newMessage, sizeof(newMessage));
```

No change to `BuildPreviewMessage` signature — passing `NULL` for og_desc already suppresses it.

---

## Step 5 — vcxproj changes

Add to the `<ItemGroup>` for `<ClCompile>`:
```xml
<ClCompile Include="settings.c" />
```

Add a new `<ItemGroup>` for the resource:
```xml
<ItemGroup>
  <ResourceCompile Include="settings.rc" />
</ItemGroup>
```

Add to `<ClInclude>`:
```xml
<ClInclude Include="resource.h" />
<ClInclude Include="settings.h" />
```

---

## Step 6 — vcxproj.filters changes

- `settings.c` → Source Files
- `settings.rc` → Resource Files
- `resource.h`, `settings.h` → Header Files

---

## Verification

1. Build Release|x64 — should compile with no errors; the `.rc` produces a `.res` that links into the DLL.
2. Install in TS3. In the plugin list, `ts3websitepreview` should now show a **Settings** button.
3. Click Settings — the dialog opens with the checkbox checked by default.
4. Uncheck and click OK — subsequent URL previews should omit the description line.
5. Recheck and click OK — description resumes.
6. Reopen TS3 — verify the setting persisted (check `%APPDATA%\TS3Client\plugins\ts3websitepreview\ts3websitepreview.ini`).
7. Run the unit test suite — all 23 tests should still pass (no signature changes).
