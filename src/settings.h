#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct {
    int show_description;
    int show_title_inline;
} PluginSettings;

extern PluginSettings g_settings;

void Settings_Load(const char* plugin_path);
void Settings_Save(const char* plugin_path);

#endif
