#include "Application.h"

#include "core/logging/Logger.h"

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
#include "services/filesystem/IRemoteFileService.h"
#include "services/filesystem/RemoteFileService.h"

#include "viewmodels/dashboard/DashboardViewModel.h"
#include "viewmodels/docker/DockerViewModel.h"
#include "viewmodels/doctor/DoctorViewModel.h"
#include "viewmodels/settings/SettingsViewModel.h"
#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/packettools/PacketToolsViewModel.h"
#include "viewmodels/logviewer/LogViewerViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"
#include "viewmodels/validator/ValidatorViewModel.h"

#include "ui/styles/ThemeManager.h"
#include "ui/dialogs/ConnectionDialog.h"

#include <QStandardPaths>
#include <QDir>

namespace OpenC3::App {

// ── Application ───────────────────────────────────────────────────────────────

Application::Application(QApplication& qtApp)
    : qtApp_(qtApp)
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

    // ── Connection dialog on startup ──────────────────────────────────────────
    auto& settingsVm = *registry_.resolve<ViewModels::SettingsViewModel>();
    {
        UI::Dialogs::ConnectionDialog dlg(settingsVm);
        dlg.exec();
    }

    // ── Main window ───────────────────────────────────────────────────────────
    mainWindow_ = std::make_unique<UI::MainWindow>(
        *registry_.resolve<ViewModels::DashboardViewModel>(),
        *registry_.resolve<ViewModels::DockerViewModel>(),
        *registry_.resolve<ViewModels::InfraViewModel>(),
        *registry_.resolve<ViewModels::DoctorViewModel>(),
        settingsVm,
        *registry_.resolve<ViewModels::PluginViewModel>(),
        *registry_.resolve<ViewModels::CmdTlmViewModel>(),
        *registry_.resolve<ViewModels::PacketToolsViewModel>(),
        *registry_.resolve<ViewModels::LogViewerViewModel>(),
        *registry_.resolve<ViewModels::ValidatorViewModel>());

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

    // Wire executor proxy: atomically swap to the live executor on connect,
    // reset to the null executor on disconnect or error.
    connection->onStateChanged([this, conn = connection.get()](
        const Services::ConnectionEvent& ev)
    {
        if (ev.state == Services::ConnectionState::Connected) {
            executorProxy_.swap(conn->executor());
            Logging::Logger::info("[App] ExecutorProxy swapped to live connection");
        } else if (ev.state == Services::ConnectionState::Disconnected
                || ev.state == Services::ConnectionState::Error) {
            executorProxy_.reset();
            Logging::Logger::info("[App] ExecutorProxy reset to NullExecutor");
        }
    });

    // ── Domain services (all hold a stable ref to ExecutorProxy) ─────────────
    auto dockerSvc = std::make_shared<Services::DockerService>(executorProxy_);
    registry_.registerInstance<Services::IDockerService>(dockerSvc);

    auto systemSvc = std::make_shared<Services::SystemService>(executorProxy_);
    registry_.registerInstance<Services::ISystemService>(systemSvc);

    // Doctor probes the COSMOS root configured on the active connection profile,
    // falling back to "/cosmos" when nothing is connected.
    auto doctorSvc = std::make_shared<Services::DoctorService>(
        executorProxy_,
        [conn = connection.get()] { return conn->cosmosRootPath(); });
    registry_.registerInstance<Services::IDoctorService>(doctorSvc);

    auto pluginSvc = std::make_shared<Services::PluginService>(executorProxy_);
    registry_.registerInstance<Services::IPluginService>(pluginSvc);

    auto fsSvc = std::make_shared<Services::RemoteFileService>(executorProxy_);
    registry_.registerInstance<Services::IRemoteFileService>(fsSvc);

    Logging::Logger::info("[App] All services registered");
}

void Application::registerViewModels()
{
    auto connection = registry_.resolve<Services::IConnectionService>();
    auto docker     = registry_.resolve<Services::IDockerService>();
    auto system     = registry_.resolve<Services::ISystemService>();
    auto doctor     = registry_.resolve<Services::IDoctorService>();
    auto settings   = registry_.resolve<Services::ISettingsService>();
    auto plugin     = registry_.resolve<Services::IPluginService>();
    auto fs         = registry_.resolve<Services::IRemoteFileService>();

    auto dashVm = std::make_shared<ViewModels::DashboardViewModel>(
        *connection, *docker, *system);
    registry_.registerInstance<ViewModels::DashboardViewModel>(dashVm);

    auto dockerVm = std::make_shared<ViewModels::DockerViewModel>(*connection, *docker);
    registry_.registerInstance<ViewModels::DockerViewModel>(dockerVm);

    auto doctorVm = std::make_shared<ViewModels::DoctorViewModel>(*doctor);
    registry_.registerInstance<ViewModels::DoctorViewModel>(doctorVm);

    auto settingsVm = std::make_shared<ViewModels::SettingsViewModel>(
        *settings, *connection);
    registry_.registerInstance<ViewModels::SettingsViewModel>(settingsVm);

    auto pluginVm = std::make_shared<ViewModels::PluginViewModel>(*plugin, *connection);
    registry_.registerInstance<ViewModels::PluginViewModel>(pluginVm);

    auto cmdTlmVm = std::make_shared<ViewModels::CmdTlmViewModel>(
        *connection, *fs);
    registry_.registerInstance<ViewModels::CmdTlmViewModel>(cmdTlmVm);

    auto packetToolsVm = std::make_shared<ViewModels::PacketToolsViewModel>(
        *connection, *fs);
    registry_.registerInstance<ViewModels::PacketToolsViewModel>(packetToolsVm);

    auto logViewerVm = std::make_shared<ViewModels::LogViewerViewModel>(
        *connection, *fs);
    registry_.registerInstance<ViewModels::LogViewerViewModel>(logViewerVm);

    auto infraVm = std::make_shared<ViewModels::InfraViewModel>(
        *connection, *fs);
    registry_.registerInstance<ViewModels::InfraViewModel>(infraVm);

    // Validator needs no services — it runs fully offline static analysis.
    auto validatorVm = std::make_shared<ViewModels::ValidatorViewModel>();
    registry_.registerInstance<ViewModels::ValidatorViewModel>(validatorVm);

    Logging::Logger::info("[App] All ViewModels registered");
}

} // namespace OpenC3::App
