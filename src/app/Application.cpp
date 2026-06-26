#include "Application.h"

#include "core/logging/Logger.h"
#include "core/connection/NullCommandExecutor.h"

#include "services/settings/ISettingsService.h"
#include "services/settings/SettingsService.h"
#include "services/connection/IConnectionService.h"
#include "services/connection/ConnectionService.h"
#include "services/docker/IDockerService.h"
#include "services/docker/DockerService.h"
#include "services/system/ISystemService.h"
#include "services/system/SystemService.h"
#include "services/doctor/IDoctorService.h"
#include "services/doctor/DoctorService.h"
#include "services/plugin/IPluginService.h"
#include "services/plugin/PluginService.h"

#include "viewmodels/dashboard/DashboardViewModel.h"
#include "viewmodels/docker/DockerViewModel.h"
#include "viewmodels/doctor/DoctorViewModel.h"
#include "viewmodels/settings/SettingsViewModel.h"

#include "ui/styles/ThemeManager.h"
#include "ui/dialogs/ConnectionDialog.h"

#include <QStandardPaths>
#include <QDir>

namespace OpenC3::App {

// ── ExecutorHolder ─────────────────────────────────────────────────────────────
// Provides a stable ICommandExecutor& to services throughout the session.
// Initially backed by NullCommandExecutor; swapped to the real executor
// after the user connects. Services hold a reference to ExecutorHolder's
// inner ref, so they automatically use the live executor after connection.
//
// This is intentionally simple for Phase 2. Phase 3 will introduce a proper
// ExecutorProxy with lock-free swap semantics.
struct ExecutorHolder {
    Core::Connection::NullCommandExecutor  nullExecutor;
    Core::Connection::ICommandExecutor*    current{&nullExecutor};

    Core::Connection::ICommandExecutor& get() noexcept { return *current; }
};

// ── Application ───────────────────────────────────────────────────────────────

Application::Application(QApplication& qtApp)
    : qtApp_(qtApp)
    , executorHolder_(std::make_shared<ExecutorHolder>())
{
    initLogging();
    Logging::Logger::info("OpenC3 Developer Toolkit starting");
}

int Application::run()
{
    UI::ThemeManager::applyDarkTheme(qtApp_);

    try {
        registerServices();
        registerViewModels();
    } catch (const std::exception& e) {
        Logging::Logger::critical("Startup wiring failed: {}", e.what());
        return 1;
    }

    // ── Optional quick-connect on startup ─────────────────────────────────────
    auto& settingsVm  = *registry_.resolve<ViewModels::SettingsViewModel>();
    const auto profiles =
        registry_.resolve<Services::ISettingsService>()->profiles();

    if (!profiles.empty()) {
        UI::Dialogs::ConnectionDialog dlg(settingsVm);
        dlg.exec(); // non-blocking on reject; app continues either way
    }

    // ── Main window ───────────────────────────────────────────────────────────
    mainWindow_ = std::make_unique<UI::MainWindow>(
        *registry_.resolve<ViewModels::DashboardViewModel>(),
        *registry_.resolve<ViewModels::DockerViewModel>(),
        *registry_.resolve<ViewModels::DoctorViewModel>(),
        settingsVm);

    mainWindow_->show();
    Logging::Logger::info("Main window shown — entering event loop");
    return qtApp_.exec();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Application::initLogging()
{
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/logs";
    QDir().mkpath(logDir);
    Logging::Logger::init((logDir + "/opencosmos.log").toStdString());
}

std::string Application::resolveSettingsPath() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return (dir + "/settings.json").toStdString();
}

void Application::registerServices()
{
    // ── Settings ──────────────────────────────────────────────────────────────
    auto settings = std::make_shared<Services::SettingsService>(
        resolveSettingsPath());
    settings->load();
    registry_.registerInstance<Services::ISettingsService>(settings);

    // ── Connection ────────────────────────────────────────────────────────────
    auto connection = std::make_shared<Services::ConnectionService>(*settings);
    registry_.registerInstance<Services::IConnectionService>(connection);

    // Wire executor swap: when connected, point ExecutorHolder at the live executor
    connection->onStateChanged([this, conn = connection.get()](
        const Services::ConnectionEvent& ev)
    {
        if (ev.state == Services::ConnectionState::Connected) {
            auto* ex = conn->executor();
            if (ex) executorHolder_->current = ex;
            Logging::Logger::info("[App] Executor swapped to live connection");
        } else if (ev.state == Services::ConnectionState::Disconnected
                || ev.state == Services::ConnectionState::Error) {
            executorHolder_->current = &executorHolder_->nullExecutor;
            Logging::Logger::info("[App] Executor reset to NullExecutor");
        }
    });

    // ── Domain services (all share the ExecutorHolder reference) ─────────────
    auto& execRef = executorHolder_->get(); // stable reference via holder

    auto dockerSvc = std::make_shared<Services::DockerService>(execRef);
    registry_.registerInstance<Services::IDockerService>(dockerSvc);

    auto systemSvc = std::make_shared<Services::SystemService>(execRef);
    registry_.registerInstance<Services::ISystemService>(systemSvc);

    auto doctorSvc = std::make_shared<Services::DoctorService>(execRef);
    registry_.registerInstance<Services::IDoctorService>(doctorSvc);

    auto pluginSvc = std::make_shared<Services::PluginService>(execRef);
    registry_.registerInstance<Services::IPluginService>(pluginSvc);

    Logging::Logger::info("[App] All services registered");
}

void Application::registerViewModels()
{
    auto connection = registry_.resolve<Services::IConnectionService>();
    auto docker     = registry_.resolve<Services::IDockerService>();
    auto system     = registry_.resolve<Services::ISystemService>();
    auto doctor     = registry_.resolve<Services::IDoctorService>();
    auto settings   = registry_.resolve<Services::ISettingsService>();

    auto dashVm = std::make_shared<ViewModels::DashboardViewModel>(
        *connection, *docker, *system);
    registry_.registerInstance<ViewModels::DashboardViewModel>(dashVm);

    auto dockerVm = std::make_shared<ViewModels::DockerViewModel>(*docker);
    registry_.registerInstance<ViewModels::DockerViewModel>(dockerVm);

    auto doctorVm = std::make_shared<ViewModels::DoctorViewModel>(*doctor);
    registry_.registerInstance<ViewModels::DoctorViewModel>(doctorVm);

    auto settingsVm = std::make_shared<ViewModels::SettingsViewModel>(
        *settings, *connection);
    registry_.registerInstance<ViewModels::SettingsViewModel>(settingsVm);

    Logging::Logger::info("[App] All ViewModels registered");
}

} // namespace OpenC3::App
