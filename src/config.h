#pragma once

#include <QDialog>
#include <QString>

// The two user-facing options (replaces the old PluginSettings struct + Win32 ini).
struct PluginSettings {
    bool show_description = true;
    bool show_title_inline = true;
};

// Load / save the options from a QSettings ini at `iniPath`
// (portable, replaces GetPrivateProfileInt / WritePrivateProfileString).
void settingsLoad(const QString& iniPath, PluginSettings& s);
void settingsSave(const QString& iniPath, const PluginSettings& s);

class QCheckBox;

// The settings dialog (replaces the native Win32 resource dialog). Shown from
// ts3plugin_configure() on the TS3 Qt thread — portable, no #ifdef.
class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    ConfigDialog(QString iniPath, PluginSettings* settings, QWidget* parent = nullptr);

private:
    void onAccept();

    QString m_iniPath;
    PluginSettings* m_settings;
    QCheckBox* m_desc;
    QCheckBox* m_inline;
};
