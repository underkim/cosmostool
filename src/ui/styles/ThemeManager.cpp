#include "ThemeManager.h"
#include "core/logging/Logger.h"

#include <QFile>
#include <QPalette>
#include <QStyle>

namespace OpenC3::UI {

void ThemeManager::applyDarkTheme(QApplication& app)
{
    // ── Qt Palette ────────────────────────────────────────────────────────────
    QPalette palette;
    palette.setColor(QPalette::Window,          QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::WindowText,      QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Base,            QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::AlternateBase,   QColor(0x2d, 0x2d, 0x2d));
    palette.setColor(QPalette::ToolTipBase,     QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ToolTipText,     QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Text,            QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Button,          QColor(0x37, 0x37, 0x37));
    palette.setColor(QPalette::ButtonText,      QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::BrightText,      Qt::white);
    palette.setColor(QPalette::Link,            QColor(0x56, 0x9c, 0xd6));
    palette.setColor(QPalette::Highlight,       QColor(0x26, 0x4f, 0x78));
    palette.setColor(QPalette::HighlightedText, Qt::white);

    // Disabled state
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(0x60, 0x60, 0x60));
    palette.setColor(QPalette::Disabled, QPalette::Text,
                     QColor(0x60, 0x60, 0x60));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(0x60, 0x60, 0x60));

    app.setPalette(palette);

    // ── QSS Stylesheet ────────────────────────────────────────────────────────
    loadStylesheet(app, ":/themes/dark.qss");
}

void ThemeManager::applyLightTheme(QApplication& app)
{
    app.setPalette(app.style()->standardPalette());
}

void ThemeManager::loadStylesheet(QApplication& app, const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        Logging::Logger::warn("[ThemeManager] Stylesheet not found: {}",
                              resourcePath.toStdString());
        return;
    }
    app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

} // namespace OpenC3::UI
