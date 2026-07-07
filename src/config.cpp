#include "config.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QSettings>
#include <QVBoxLayout>

void settingsLoad(const QString& iniPath, PluginSettings& s) {
    QSettings ini(iniPath, QSettings::IniFormat);
    s.show_description  = ini.value(QStringLiteral("Settings/ShowDescription"),  true).toBool();
    s.show_title_inline = ini.value(QStringLiteral("Settings/ShowTitleInline"), true).toBool();
}

void settingsSave(const QString& iniPath, const PluginSettings& s) {
    QSettings ini(iniPath, QSettings::IniFormat);
    ini.setValue(QStringLiteral("Settings/ShowDescription"),  s.show_description);
    ini.setValue(QStringLiteral("Settings/ShowTitleInline"), s.show_title_inline);
    ini.sync();
}

ConfigDialog::ConfigDialog(QString iniPath, PluginSettings* settings, QWidget* parent)
    : QDialog(parent), m_iniPath(std::move(iniPath)), m_settings(settings) {
    setWindowTitle(QStringLiteral("TS3 Website Preview Settings"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_desc   = new QCheckBox(QStringLiteral("Show page description in chat"), this);
    m_inline = new QCheckBox(QStringLiteral("Show title inline in typed messages"), this);
    m_desc->setChecked(m_settings->show_description);
    m_inline->setChecked(m_settings->show_title_inline);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_desc);
    layout->addWidget(m_inline);
    layout->addWidget(buttons);
    // Size the window to its content (sizeHint) and lock it there — non-resizable on
    // every platform. (The template's Qt::MSWindowsFixedSizeDialogHint is Windows-only.)
    layout->setSizeConstraint(QLayout::SetFixedSize);
    // Drop the "?" context-help title-bar button, like the template's dialog.
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(buttons, &QDialogButtonBox::accepted, this, &ConfigDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConfigDialog::onAccept() {
    m_settings->show_description  = m_desc->isChecked();
    m_settings->show_title_inline = m_inline->isChecked();
    settingsSave(m_iniPath, *m_settings);
    accept();
}
