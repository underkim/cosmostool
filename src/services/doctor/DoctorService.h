#pragma once

#include "IDoctorService.h"
#include "core/api/ICosmosApiClient.h"
#include "core/connection/ICommandExecutor.h"

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenC3::Services {

/// Health checks across three layers.
///
/// The original implementation only had layer 1 - it verified that Docker was
/// installed and that the machine had CPU/memory/disk, which says nothing
/// about whether COSMOS works. Its one COSMOS-specific container check counted
/// `docker ps --filter name=cosmos`, a filter that misses most of the stack:
/// every service in COSMOS 6.10.4's compose.yaml is named openc3-* (minio,
/// redis, redis-ephemeral, traefik, operator, cmd-tlm-api, script-runner-api,
/// init), so "cosmos" only matched the two openc3-cosmos-* API containers plus
/// init. It also assumed a fixed compose project name, which .env can change.
///
///   L1 Infrastructure  Docker, CPU, memory, disk, network, clock (shell)
///   L2 COSMOS stack    Each expected container up; Redis and MinIO reachable
///                      (shell, via `docker ps` / `docker exec`)
///   L3 COSMOS logic    API reachable and authenticated; scope present;
///                      plugins installed; microservices alive and not
///                      erroring; interfaces actually connected (not just
///                      defined) - all through the HTTP API
///
/// checkMicroservices() and checkInterfaces() go beyond "does the model
/// exist" to actual runtime state: microservice_status#show
/// (MicroserviceStatusModel) reports 'error' when a microservice's own
/// thread has crashed even though its definition is still present, and the
/// JSON-DRB get_all_interface_info call reports each interface's live
/// CONNECTED/DISCONNECTED state (InterfaceStatusModel) - information the
/// plain GET /interfaces route (InterfaceModel only) does not carry at all.
/// Both were the two "looks configured but isn't actually working" classes
/// of failure the count-only versions of these checks could not catch.
///
/// L3 needs the API transport, so the client is injected alongside the shell
/// executor. When no API endpoint is configured those checks report Skipped
/// rather than Fail - a shell-only profile is a valid configuration, not a
/// broken one.
class DoctorService final : public IDoctorService {
public:
    /// @param executor            command executor (proxy in production).
    /// @param api                 COSMOS API client (proxy in production).
    /// @param cosmosRootProvider  returns the COSMOS root path to probe. Lets the
    ///        OpenC3 checks follow the connected profile's configured path instead
    ///        of a hardcoded location. When empty, defaults to "/cosmos".
    explicit DoctorService(
        Core::Connection::ICommandExecutor& executor,
        Core::Api::ICosmosApiClient&        api,
        std::function<std::string()>        cosmosRootProvider = {});

    [[nodiscard]] Models::DoctorReport runAll(
        std::function<void(const Models::HealthCheckResult&)> onProgress = {}) override;

    [[nodiscard]] Models::HealthCheckResult
        runCheck(const std::string& checkId) override;

private:
    using CheckFn = std::function<Models::HealthCheckResult()>;

    void registerChecks();

    // ── L1: infrastructure ────────────────────────────────────────────────────
    Models::HealthCheckResult checkDockerInstalled();
    Models::HealthCheckResult checkDockerRunning();
    Models::HealthCheckResult checkDockerCompose();
    Models::HealthCheckResult checkCpuUsage();
    Models::HealthCheckResult checkMemoryUsage();
    Models::HealthCheckResult checkDiskSpace();
    Models::HealthCheckResult checkNetworkConnectivity();
    Models::HealthCheckResult checkTimeSynchronization();

    // ── L2: COSMOS stack ──────────────────────────────────────────────────────
    Models::HealthCheckResult checkCosmosContainers();
    Models::HealthCheckResult checkRedis();
    Models::HealthCheckResult checkRedisEphemeral();
    Models::HealthCheckResult checkMinio();
    Models::HealthCheckResult checkOpenC3Version();

    // ── L3: COSMOS logic (API) ────────────────────────────────────────────────
    Models::HealthCheckResult checkApiReachable();
    Models::HealthCheckResult checkScope();
    Models::HealthCheckResult checkPlugins();
    Models::HealthCheckResult checkMicroservices();
    Models::HealthCheckResult checkInterfaces();
    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Names of the running containers, one per line. Uses no name filter -
    /// the caller matches substrings instead, so a renamed compose project
    /// (COMPOSE_PROJECT_NAME in .env) does not hide the whole stack.
    [[nodiscard]] std::vector<std::string> runningContainers();

    /// Finds a running container whose name contains `fragment`, so both
    /// "cosmos-openc3-redis-1" and "openc3-redis" match "openc3-redis".
    /// Returns an empty string when nothing matches.
    [[nodiscard]] std::string findContainer(const std::string& fragment);

    /// Shared shape for an L3 check when the API is not configured/connected.
    [[nodiscard]] static Models::HealthCheckResult apiUnavailable(
        std::string checkId, std::string name);

    Core::Connection::ICommandExecutor& executor_;
    Core::Api::ICosmosApiClient&        api_;
    std::function<std::string()>        cosmosRootProvider_;
    std::vector<std::pair<std::string, CheckFn>> checks_; // ordered
    std::unordered_map<std::string, CheckFn>     checkMap_;

    // Request id for the JSON-DRB call checkInterfaces() makes
    // (get_all_interface_info) - only needs to be unique within this
    // service's lifetime; COSMOS simply echoes it back.
    std::atomic<int> nextRequestId_{1};
};

} // namespace OpenC3::Services
