#pragma once

#include "ICosmosCommandService.h"
#include "core/api/ICosmosApiClient.h"

#include <atomic>

namespace OpenC3::Services {

/// ICosmosCommandService over COSMOS's JSON-DRB API (same single POST
/// endpoint as CosmosRuntimeService, /openc3-api/api).
class CosmosCommandService final : public ICosmosCommandService {
public:
    explicit CosmosCommandService(Core::Api::ICosmosApiClient& api);

    [[nodiscard]] Models::CommandSendResult send(
        const std::string& targetName,
        const std::string& commandName,
        const std::vector<CommandParameter>& parameters) override;

    [[nodiscard]] Models::CommandSendResult sendConfirmed(
        const std::string& targetName,
        const std::string& commandName,
        const std::vector<CommandParameter>& parameters) override;

    [[nodiscard]] bool isAvailable() const override;

private:
    [[nodiscard]] Models::CommandSendResult sendVia(
        const char*                          method,
        const std::string&                   targetName,
        const std::string&                   commandName,
        const std::vector<CommandParameter>& parameters);

    Core::Api::ICosmosApiClient& api_;
    std::atomic<int>             nextRequestId_{1};
};

} // namespace OpenC3::Services
