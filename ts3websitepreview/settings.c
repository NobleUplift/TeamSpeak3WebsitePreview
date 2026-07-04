#include "settings.h"
#include <windows.h>
#include <stdio.h>

PluginSettings g_settings = { 1, 1 };

static void GetIniPath(const char* plugin_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%sts3websitepreview.ini", plugin_path);
}

void Settings_Load(const char* plugin_path) {
    char ini[MAX_PATH];
    GetIniPath(plugin_path, ini, sizeof(ini));
    g_settings.show_description  = GetPrivateProfileIntA("Settings", "ShowDescription",  1, ini);
    g_settings.show_title_inline = GetPrivateProfileIntA("Settings", "ShowTitleInline",  1, ini);
}

void Settings_Save(const char* plugin_path) {
    char ini[MAX_PATH];
    char val[4];
    GetIniPath(plugin_path, ini, sizeof(ini));
    snprintf(val, sizeof(val), "%d", g_settings.show_description);
    WritePrivateProfileStringA("Settings", "ShowDescription", val, ini);
    snprintf(val, sizeof(val), "%d", g_settings.show_title_inline);
    WritePrivateProfileStringA("Settings", "ShowTitleInline", val, ini);
}
