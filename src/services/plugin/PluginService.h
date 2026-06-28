#pragma once

#include "IPluginService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

class PluginService final : public IPluginService {
public:
    explicit PluginService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] std::vector<Models::Plugin>
        listInstalled(const std::string& cosmosRoot) override;

    [[nodiscard]] Models::PluginValidationResult
        validate(const std::string& localPluginPath) override;

    [[nodiscard]] bool install(
        const std::string& gemFilePath,
        const std::string& cosmosRoot) override;

    [[nodiscard]] bool remove(
        const std::string& pluginName,
        const std::string& cosmosRoot) override;

    [[nodiscard]] std::string build(
        const std::string& pluginRootPath) override;

    [[nodiscard]] bool backup(
        const std::string& pluginName,
        const std::string& localBackupPath,
        const std::string& cosmosRoot) override;

private:
    Core::Connection::ICommandExecutor& executor_;
};

} // namespace OpenC3::Services
