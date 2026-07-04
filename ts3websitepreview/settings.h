#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct {
    int show_description;
} PluginSettings;

extern PluginSettings g_settings;

void Settings_Load(const char* plugin_path);
void Settings_Save(const char* plugin_path);

#endif
