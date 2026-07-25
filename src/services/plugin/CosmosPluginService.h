#pragma once

#include "ICosmosPluginService.h"
#include "core/api/ICosmosApiClient.h"

namespace OpenC3::Services {

/// ICosmosPluginService over the COSMOS HTTP API.
///
/// Holds a reference to an api client (in practice Core::Api::ApiClientProxy,
/// whose identity is stable across connect/disconnect) rather than owning one,
/// mirroring how the shell-based services take an ExecutorProxy&.
class CosmosPluginService final : public ICosmosPluginService {
public:
    explicit CosmosPluginService(Core::Api::ICosmosApiClient& api);

    [[nodiscard]] std::vector<Models::Plugin> listInstalled() override;

    [[nodiscard]] PluginOperationResult uploadGem(
        const std::string& localGemPath) override;

    [[nodiscard]] PluginOperationResult installUploaded(
        const std::string& pluginId,
        const std::string& pluginHashJson) override;

    [[nodiscard]] PluginOperationResult installGem(
        const std::string& localGemPath) override;

    [[nodiscard]] PluginOperationResult uninstall(
        const std::string& pluginId) override;

    [[nodiscard]] PluginJobStatus jobStatus(
        const std::string& processName) override;

    [[nodiscard]] bool isAvailable() const override;

private:
    Core::Api::ICosmosApiClient& api_;
};

} // namespace OpenC3::Services
