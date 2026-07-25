#pragma once

#include "IScriptService.h"
#include "core/api/ICosmosApiClient.h"

namespace OpenC3::Services {

/// IScriptService over the COSMOS Script Runner HTTP API (script-api).
class CosmosScriptService final : public IScriptService {
public:
    explicit CosmosScriptService(Core::Api::ICosmosApiClient& api);

    [[nodiscard]] std::vector<Models::CosmosScript> listScripts(
        const std::string& targetFilter = {}) override;

    [[nodiscard]] Models::ScriptRunResult run(
        const std::string& scriptPath) override;

    [[nodiscard]] Models::ScriptExecutionStatus status(
        const std::string& runningScriptId) override;

    [[nodiscard]] bool stop(const std::string& runningScriptId) override;
    [[nodiscard]] bool resume(const std::string& runningScriptId) override;
    [[nodiscard]] bool pause(const std::string& runningScriptId) override;

    [[nodiscard]] bool isAvailable() const override;

private:
    Core::Api::ICosmosApiClient& api_;
};

} // namespace OpenC3::Services
