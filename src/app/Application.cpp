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
#include "services/plugin/ICosmosPluginService.h"
#include "services/plugin/CosmosPluginService.h"
#include "services/script/IScriptService.h"
#include "services/script/CosmosScriptService.h"
#include "services/runtime/ICosmosRuntimeService.h"
#include "services/runtime/CosmosRuntimeService.h"
#include "services/workspace/IWorkspaceService.h"
#include "services/workspace/WorkspaceService.h"
#include "services/filesystem/IRemoteFileService.h"
#include "services/filesystem/RemoteFileService.h"

#include "viewmodels/dashboard/DashboardViewModel.h"
#include "viewmodels/docker/DockerViewModel.h"
#include "viewmodels/doctor/DoctorViewModel.h"
#include "viewmodels/settings/SettingsViewModel.h"
#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/script/ScriptViewModel.h"
#include "viewmodels/runtime/RuntimeViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/packettools/PacketToolsViewModel.h"
#include "viewmodels/logviewer/LogViewerViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"
#include "viewmodels/validator/ValidatorViewModel.h"

#include "ui/styles/ThemeManager.h"
#include "ui/dialogs/ConnectionDialog.h"
#include "viewmodels/api/HttpCosmosApiClient.h"
#include "StartupPolicy.h"

#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QLibraryInfo>

namespace OpenC3::App {

// ── Application ───────────────────────────────────────────────────────────────

Application::Application(QApplication& qtApp)
    : qtApp_(qtApp)
{
    initLogging();
    Logging::Logger::info("OpenC3 Developer Toolkit starting");
}

Application::~Application()
{
    // Order matters. Member destruction runs in reverse declaration order,
    // which would destroy apiClient_ while apiProxy_ (declared earlier, so
    // destroyed later) may still point at it. Any service that outlives this
    // window and touches the proxy would then dereference freed memory.
    // Detaching first makes the proxy fall back to its embedded null client,
    // so the teardown order becomes irrelevant.
    apiProxy_.reset();
    executorProxy_.reset();
}

void Application::loadLanguage()
{
    // Plain QSettings (same store as MainWindow's window geometry/state), not
    // the JSON-backed SettingsService - this is app chrome preference, not a
    // COSMOS connection setting. "en" (default) needs no QTranslator at all;
    // switching to "ko" takes effect on the next launch (this app has no
    // dynamic-retranslation wiring), which SettingsView's language control
    // tells the user explicitly.
    const QSettings appSettings;
    const QString language = appSettings.value("App/language", "en").toString();
    if (language != "ko")
        return;

    // Embedded in the executable itself (see src/app/CMakeLists.txt's
    // qt_add_resources call) rather than loaded from a loose file next to
    // the binary - a "portable" copy of just the .exe would otherwise leave
    // the translation behind and silently fall back to English.
    const QString qmPath = ":/translations/openc3_ko.qm";
    if (translator_.load(qmPath))
        qtApp_.installTranslator(&translator_);
    else
        Logging::Logger::warn("Korean translation resource not found at {}", qmPath.toStdString());
}

int Application::run()
{
    loadLanguage();
    UI::ThemeManager::applyDarkTheme(qtApp_);

    try {
        registerServices();
        registerViewModels();
    } catch (const std::exception& e) {
        Logging::Logger::critical("Startup wiring failed: {}", e.what());
        return 1;
    }

    // ── Connection dialog on startup ──────────────────────────────────────────
    // Plugin Creation mode skips this entirely - MainWindow's own
    // autoConnectIfNeeded() connects silently once shown, so the user never
    // has to clear a dialog before they can start editing a plugin.
    auto& settingsVm = *registry_.resolve<ViewModels::SettingsViewModel>();
    const bool hasAnyProfile =
        !registry_.resolve<Services::ISettingsService>()->profiles().empty();
    if (shouldShowStartupConnectionDialog(UI::MainWindow::loadPersistedAppMode(), hasAnyProfile)) {
        // The startup dialog may be the only visible window. When the user clicks
        // Skip, Qt would otherwise mark the application for quit before the main
        // window is created, causing the app to exit immediately in an offline /
        // no-COSMOS environment. Keep the application alive while the startup
        // dialog is dismissed, then restore the normal last-window behavior.
        const bool quitOnLastWindowClosed = qtApp_.quitOnLastWindowClosed();
        qtApp_.setQuitOnLastWindowClosed(false);

        UI::Dialogs::ConnectionDialog dlg(settingsVm);
        dlg.exec();

        qtApp_.setQuitOnLastWindowClosed(quitOnLastWindowClosed);
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
        *registry_.resolve<ViewModels::ValidatorViewModel>(),
        *registry_.resolve<ViewModels::ScriptViewModel>(),
        *registry_.resolve<ViewModels::RuntimeViewModel>());

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
    // reset to the null executor on disconnect, error, or (re)connect attempt.
    //
    // Connecting is handled the same as Disconnected/Error - not just for
    // symmetry, but because ConnectionService::connect() reassigns its
    // executor_ unique_ptr (destroying whatever executor was previously
    // connected) *after* firing this Connecting event but *before* firing
    // Connected. Resetting the proxy here first means that reassignment
    // never destroys an executor the proxy is still pointing at - the same
    // hazard the Disconnected/Error branch guards against.
    //
    // The COSMOS API client follows the same lifecycle. It is a separate
    // transport (see CLAUDE.md's "Two transports"), but it is configured from
    // the same profile and becomes valid/invalid at the same moments, so
    // driving both from one callback keeps them from drifting apart.
    apiClient_ = std::make_unique<ViewModels::Api::HttpCosmosApiClient>();

    connection->onStateChanged([this, conn = connection.get()](
        const Services::ConnectionEvent& ev)
    {
        if (ev.state == Services::ConnectionState::Connected) {
            executorProxy_.swap(conn->executor());
            Logging::Logger::info("[App] ExecutorProxy swapped to live connection");

            // Only bring the API up when the active profile actually carries
            // credentials. Without a password every COSMOS route fails
            // authorization, so connecting anyway would leave the UI showing
            // authentication errors for a profile that simply never opted in
            // to the API - shell-only profiles stay silently shell-only.
            const auto profile = conn->activeProfile();
            if (profile && profile->hasApiEndpoint()) {
                apiClient_->configure(profile->toApiConfig());
                if (apiClient_->connect()) {
                    apiProxy_.swap(apiClient_.get());
                    Logging::Logger::info(
                        "[App] COSMOS API connected at {}",
                        profile->resolvedApiBaseUrl());
                } else {
                    // A failed API probe must not take the shell connection
                    // down with it: Docker control and file editing still work
                    // without the API, and COSMOS-backed views degrade to
                    // "unavailable" through NullCosmosApiClient.
                    apiProxy_.reset();
                    Logging::Logger::warn(
                        "[App] COSMOS API unavailable: {}", apiClient_->lastError());
                }
            } else {
                apiProxy_.reset();
                Logging::Logger::info(
                    "[App] Profile has no COSMOS API password; API features disabled");
            }
        } else if (ev.state == Services::ConnectionState::Disconnected
                || ev.state == Services::ConnectionState::Error
                || ev.state == Services::ConnectionState::Connecting) {
            executorProxy_.reset();
            apiProxy_.reset();
            Logging::Logger::info("[App] ExecutorProxy reset to NullExecutor");
        }
    });

    // ── Domain services (all hold a stable ref to ExecutorProxy) ─────────────
    auto dockerSvc = std::make_shared<Services::DockerService>(executorProxy_);
    registry_.registerInstance<Services::IDockerService>(dockerSvc);

    auto systemSvc = std::make_shared<Services::SystemService>(
        executorProxy_,
        [conn = connection.get()] { return conn->cosmosRootPath(); });
    registry_.registerInstance<Services::ISystemService>(systemSvc);

    // Doctor probes the COSMOS root configured on the active connection profile,
    // falling back to "/cosmos" when nothing is connected. It takes both
    // transports: shell for infrastructure/stack checks, API for COSMOS's own
    // logical state (scope, plugins, microservices, interfaces).
    auto doctorSvc = std::make_shared<Services::DoctorService>(
        executorProxy_,
        apiProxy_,
        [conn = connection.get()] { return conn->cosmosRootPath(); });
    registry_.registerInstance<Services::IDoctorService>(doctorSvc);

    auto workspaceSvc = std::make_shared<Services::WorkspaceService>(executorProxy_);
    registry_.registerInstance<Services::IWorkspaceService>(workspaceSvc);

    // COSMOS-side plugin lifecycle (API transport). Separate from
    // WorkspaceService, which only sees plugin *sources* on the host
    // filesystem - see ICosmosPluginService's header for why both exist.
    auto cosmosPluginSvc = std::make_shared<Services::CosmosPluginService>(apiProxy_);
    registry_.registerInstance<Services::ICosmosPluginService>(cosmosPluginSvc);

    // COSMOS Script Runner (script-api transport). Lists/runs/tracks scripts
    // actually deployed in the connected scope's bucket storage - unrelated
    // to the shell-based "Run Script" quick check PluginView also offers.
    auto scriptSvc = std::make_shared<Services::CosmosScriptService>(apiProxy_);
    registry_.registerInstance<Services::IScriptService>(scriptSvc);

    // Read-only target/command/telemetry state (JSON-DRB API transport, one
    // POST endpoint dispatched by method name). Command *sending* stays out
    // of scope until it has its own safety UX - see ICosmosRuntimeService's
    // header.
    auto runtimeSvc = std::make_shared<Services::CosmosRuntimeService>(apiProxy_);
    registry_.registerInstance<Services::ICosmosRuntimeService>(runtimeSvc);

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
    auto workspace  = registry_.resolve<Services::IWorkspaceService>();
    auto cosmosPlug = registry_.resolve<Services::ICosmosPluginService>();
    auto scriptSvc  = registry_.resolve<Services::IScriptService>();
    auto runtimeSvc = registry_.resolve<Services::ICosmosRuntimeService>();
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

    auto pluginVm = std::make_shared<ViewModels::PluginViewModel>(
        *workspace, *cosmosPlug, *connection);
    registry_.registerInstance<ViewModels::PluginViewModel>(pluginVm);

    auto scriptVm = std::make_shared<ViewModels::ScriptViewModel>(*scriptSvc);
    registry_.registerInstance<ViewModels::ScriptViewModel>(scriptVm);

    auto runtimeVm = std::make_shared<ViewModels::RuntimeViewModel>(*runtimeSvc);
    registry_.registerInstance<ViewModels::RuntimeViewModel>(runtimeVm);

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
