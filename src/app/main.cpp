#include "Application.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    // ── Qt high-DPI support (Qt6 enables this by default) ─────────────────────
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("OpenC3DevToolkit");
    app.setApplicationDisplayName("OpenC3 Developer Toolkit");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenC3");
    app.setOrganizationDomain("openc3.com");

    OpenC3::App::Application toolkit(app);
    return toolkit.run();
}
