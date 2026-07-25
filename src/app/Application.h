#pragma once

#include "ServiceRegistry.h"
#include "core/api/ApiClientProxy.h"
#include "core/connection/ExecutorProxy.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QTranslator>
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
    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void initLogging();
    void registerServices();
    void registerViewModels();
    void loadLanguage();

    [[nodiscard]] std::string resolveSettingsPath() const;

    QApplication&                              qtApp_;
    ServiceRegistry                            registry_;
    Core::Connection::ExecutorProxy            executorProxy_;

    // Second transport. Like executorProxy_, its address is stable for the
    // lifetime of the app so services can hold a reference across
    // connect/disconnect; the concrete client behind it is swapped atomically.
    Core::Api::ApiClientProxy                  apiProxy_;

    // The live API client. Owned here rather than by the registry because
    // apiProxy_ points at it, so it must outlive every service that holds the
    // proxy - and must be destroyed only after the proxy is reset.
    std::unique_ptr<Core::Api::ICosmosApiClient> apiClient_;

    std::unique_ptr<UI::MainWindow>            mainWindow_;
    QTranslator                                translator_;
};

} // namespace OpenC3::App
