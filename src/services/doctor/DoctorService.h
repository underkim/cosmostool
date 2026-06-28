#pragma once

#include "IDoctorService.h"
#include "core/connection/ICommandExecutor.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace OpenC3::Services {

class DoctorService final : public IDoctorService {
public:
    /// @param executor            command executor (proxy in production).
    /// @param cosmosRootProvider  returns the COSMOS root path to probe. Lets the
    ///        OpenC3 checks follow the connected profile's configured path instead
    ///        of a hardcoded location. When empty, defaults to "/cosmos".
    explicit DoctorService(
        Core::Connection::ICommandExecutor& executor,
        std::function<std::string()>        cosmosRootProvider = {});

    [[nodiscard]] Models::DoctorReport runAll(
        std::function<void(const Models::HealthCheckResult&)> onProgress = {}) override;

    [[nodiscard]] Models::HealthCheckResult
        runCheck(const std::string& checkId) override;

private:
    using CheckFn = std::function<Models::HealthCheckResult()>;

    void registerChecks();
    Models::HealthCheckResult checkDockerInstalled();
    Models::HealthCheckResult checkDockerRunning();
    Models::HealthCheckResult checkDockerCompose();
    Models::HealthCheckResult checkCpuUsage();
    Models::HealthCheckResult checkMemoryUsage();
    Models::HealthCheckResult checkDiskSpace();
    Models::HealthCheckResult checkOpenC3Containers();
    Models::HealthCheckResult checkNetworkConnectivity();
    Models::HealthCheckResult checkTimeSynchronization();
    Models::HealthCheckResult checkOpenC3PluginFolder();
    Models::HealthCheckResult checkOpenC3Version();

    Core::Connection::ICommandExecutor& executor_;
    std::function<std::string()>        cosmosRootProvider_;
    std::vector<std::pair<std::string, CheckFn>> checks_; // ordered
    std::unordered_map<std::string, CheckFn>     checkMap_;
};

} // namespace OpenC3::Services
