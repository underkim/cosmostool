// Startup wiring check.
//
// Runs Application's real composition root - registerServices() /
// registerViewModels(), including the new COSMOS API proxy wiring - and then
// exits, instead of entering the Qt event loop. Exists because the shipped
// executable is built WIN32_EXECUTABLE, so its console sink goes nowhere and
// its file sink uses flush_on(warn); a GUI run killed by a timeout therefore
// produces no evidence at all that startup succeeded.
//
// This links the same libraries the app does and instantiates the same
// objects, so a wiring regression (a null service, a bad DI registration, a
// lifetime bug in the api client/proxy pair) shows up here as a non-zero exit
// with a printed reason.
#include "core/api/ApiClientProxy.h"
#include "core/connection/ExecutorProxy.h"
#include "services/connection/ConnectionService.h"
#include "services/docker/DockerService.h"
#include "services/doctor/DoctorService.h"
#include "services/filesystem/RemoteFileService.h"
#include "services/plugin/CosmosPluginService.h"
#include "services/workspace/WorkspaceService.h"
#include "services/settings/SettingsService.h"
#include "services/system/SystemService.h"
#include "viewmodels/api/HttpCosmosApiClient.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <iostream>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const std::string& label)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << label << "\n";
    if (!condition) ++failures;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::cerr << "could not create temp dir\n";
        return 2;
    }
    const std::string settingsPath =
        (tempDir.path() + "/settings.json").toStdString();

    std::cout << "-- composition root --\n";

    OpenC3::Core::Connection::ExecutorProxy executorProxy;
    OpenC3::Core::Api::ApiClientProxy       apiProxy;

    // Before any connection exists, both transports must be safe to call.
    check(!executorProxy.isConnected(), "executor proxy starts disconnected");
    check(!apiProxy.isConnected(), "api proxy starts disconnected");
    check(!apiProxy.get("/openc3-api/plugins", {}),
          "api call before connect fails cleanly instead of crashing");

    auto settings = std::make_shared<OpenC3::Services::SettingsService>(settingsPath);
    settings->load();
    auto connection = std::make_shared<OpenC3::Services::ConnectionService>(*settings);

    auto docker = std::make_shared<OpenC3::Services::DockerService>(executorProxy);
    auto system = std::make_shared<OpenC3::Services::SystemService>(
        executorProxy, [c = connection.get()] { return c->cosmosRootPath(); });
    auto doctor = std::make_shared<OpenC3::Services::DoctorService>(
        executorProxy, apiProxy,
        [c = connection.get()] { return c->cosmosRootPath(); });
    auto workspace = std::make_shared<OpenC3::Services::WorkspaceService>(executorProxy);
    auto files  = std::make_shared<OpenC3::Services::RemoteFileService>(executorProxy);

    // The service under test in step 2.
    auto cosmosPlugin =
        std::make_shared<OpenC3::Services::CosmosPluginService>(apiProxy);

    check(docker != nullptr && system != nullptr && doctor != nullptr,
          "shell-backed services construct");
    check(workspace != nullptr && files != nullptr,
          "workspace/file services construct");
    check(cosmosPlugin != nullptr, "CosmosPluginService constructs");

    std::cout << "-- disconnected behaviour --\n";

    // The whole point of the null object: every COSMOS call is safe and
    // honest before a connection exists.
    check(!cosmosPlugin->isAvailable(),
          "CosmosPluginService reports unavailable while disconnected");
    check(cosmosPlugin->listInstalled().empty(),
          "listInstalled returns empty rather than throwing");
    check(!cosmosPlugin->installGem("/tmp/nonexistent.gem"),
          "installGem fails cleanly");
    check(!cosmosPlugin->uninstall("demo__1"), "uninstall fails cleanly");

    std::cout << "-- api client lifecycle --\n";

    // Mirrors Application's wiring: configure, swap in, then detach.
    auto apiClient = std::make_unique<OpenC3::ViewModels::Api::HttpCosmosApiClient>();
    OpenC3::Core::Api::ApiConfig config;
    config.baseUrl  = "http://127.0.0.1:1";  // guaranteed refused
    config.password = "pw";
    config.scope    = "DEFAULT";
    apiClient->configure(config);

    check(!apiClient->connect(),
          "connect against a dead endpoint fails rather than hanging");
    check(!apiClient->lastError().empty(), "failure reports a reason");

    apiProxy.swap(apiClient.get());
    check(!cosmosPlugin->isAvailable(),
          "a failed connect does not make the service claim availability");

    // Application's destructor does exactly this so the proxy never outlives
    // the client it points at.
    apiProxy.reset();
    apiClient.reset();
    check(!apiProxy.isConnected(),
          "proxy falls back to null client after the real client is destroyed");
    check(!apiProxy.get("/openc3-api/plugins", {}),
          "calls after teardown are still safe");

    std::cout << (failures == 0 ? "\nSTARTUP WIRING OK\n" : "\nWIRING FAILURES\n");
    return failures == 0 ? 0 : 1;
}
