#pragma once

#include "ICosmosRuntimeService.h"
#include "core/api/ICosmosApiClient.h"

#include <atomic>

namespace OpenC3::Services {

/// ICosmosRuntimeService over COSMOS's JSON-DRB API (a single POST endpoint,
/// /openc3-api/api, dispatched by method name - see CosmosRoutes::kApiDrb).
class CosmosRuntimeService final : public ICosmosRuntimeService {
public:
    explicit CosmosRuntimeService(Core::Api::ICosmosApiClient& api);

    [[nodiscard]] std::vector<Models::RuntimeTarget> listTargets() override;

    [[nodiscard]] std::vector<Models::RuntimePacket> listCommands(
        const std::string& targetName) override;

    [[nodiscard]] std::vector<Models::RuntimePacket> listTelemetry(
        const std::string& targetName) override;

    [[nodiscard]] std::vector<Models::TelemetryItemValue> telemetryValues(
        const std::string& targetName, const std::string& packetName) override;

    [[nodiscard]] bool isAvailable() const override;

private:
    Core::Api::ICosmosApiClient& api_;
    std::atomic<int>             nextRequestId_{1};
};

} // namespace OpenC3::Services
