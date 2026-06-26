#pragma once

#include <QApplication>
#include <QString>

namespace OpenC3::UI {

class ThemeManager {
public:
    static void applyDarkTheme(QApplication& app);
    static void applyLightTheme(QApplication& app);

private:
    static void loadStylesheet(QApplication& app, const QString& resourcePath);
};

} // namespace OpenC3::UI
