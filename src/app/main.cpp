#include "Application.h"

#include <QApplication>
#include <QMessageBox>

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

    // Startup wiring (services/viewmodels) already catches std::exception, but
    // nothing upstream of that did: a failure during logger/setup construction,
    // or any exception thrown while building the startup dialog, would abort
    // the process with std::terminate() before any window ever appeared — and
    // a packaged GUI-subsystem .exe has no console to show why. Surface it.
    try {
        OpenC3::App::Application toolkit(app);
        return toolkit.run();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "OpenC3 Developer Toolkit - Startup Failed",
            QString("The application failed to start:\n\n%1").arg(e.what()));
        return 1;
    } catch (...) {
        QMessageBox::critical(nullptr, "OpenC3 Developer Toolkit - Startup Failed",
            "The application failed to start due to an unknown error.");
        return 1;
    }
}
