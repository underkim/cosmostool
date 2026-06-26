#pragma once

#include "ServiceRegistry.h"
#include "core/connection/ExecutorProxy.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <memory>
#include <string>

namespace OpenC3::App {

/// Top-level application class.
///
/// Responsibilities:
///   1. Initialise logging.
///   2. Resolve the settings file path.
///   3. Wire all services through ServiceRegistry (DI composition root).
///   4. Create ViewModels.
///   5. Show MainWindow (optionally preceded by ConnectionDialog).
///   6. Enter Qt event loop.
class Application {
public:
    explicit Application(QApplication& qtApp);
    ~Application() = default;

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void initLogging();
    void registerServices();
    void registerViewModels();

    [[nodiscard]] std::string resolveSettingsPath() const;

    QApplication&                              qtApp_;
    ServiceRegistry                            registry_;
    Core::Connection::ExecutorProxy            executorProxy_;
    std::unique_ptr<UI::MainWindow>            mainWindow_;
};

} // namespace OpenC3::App
