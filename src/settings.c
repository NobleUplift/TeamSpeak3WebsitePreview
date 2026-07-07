#include "settings.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

PluginSettings g_settings = { 1, 1 };

static void GetIniPath(const char* plugin_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%sts3websitepreview.ini", plugin_path);
}

void Settings_Load(const char* plugin_path) {
    char ini[MAX_PATH];
    GetIniPath(plugin_path, ini, sizeof(ini));
#ifdef _WIN32
    g_settings.show_description  = GetPrivateProfileIntA("Settings", "ShowDescription",  1, ini);
    g_settings.show_title_inline = GetPrivateProfileIntA("Settings", "ShowTitleInline",  1, ini);
#else
    /* Portable INI reader: scan for "Key=Value" (spaces optional). Defaults stay
     * as initialized above if the file is absent or a key is missing. */
    {
        FILE* f = fopen(ini, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                int v;
                if (sscanf(line, "ShowDescription = %d", &v) == 1 ||
                    sscanf(line, "ShowDescription=%d",   &v) == 1) {
                    g_settings.show_description = v;
                } else if (sscanf(line, "ShowTitleInline = %d", &v) == 1 ||
                           sscanf(line, "ShowTitleInline=%d",   &v) == 1) {
                    g_settings.show_title_inline = v;
                }
            }
            fclose(f);
        }
    }
#endif
}

void Settings_Save(const char* plugin_path) {
    char ini[MAX_PATH];
    GetIniPath(plugin_path, ini, sizeof(ini));
#ifdef _WIN32
    {
        char val[4];
        snprintf(val, sizeof(val), "%d", g_settings.show_description);
        WritePrivateProfileStringA("Settings", "ShowDescription", val, ini);
        snprintf(val, sizeof(val), "%d", g_settings.show_title_inline);
        WritePrivateProfileStringA("Settings", "ShowTitleInline", val, ini);
    }
#else
    {
        FILE* f = fopen(ini, "w");
        if (f) {
            fprintf(f, "[Settings]\nShowDescription=%d\nShowTitleInline=%d\n",
                    g_settings.show_description, g_settings.show_title_inline);
            fclose(f);
        }
    }
#endif
}
