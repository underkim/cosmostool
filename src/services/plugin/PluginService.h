#pragma once

#include "IPluginService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

class PluginService final : public IPluginService {
public:
    explicit PluginService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] std::vector<Models::Plugin> listInstalled() override;

    [[nodiscard]] Models::PluginValidationResult
        validate(const std::string& localPluginPath) override;

    [[nodiscard]] bool install(const std::string& gemFilePath) override;
    [[nodiscard]] bool remove(const std::string& pluginName)   override;

    [[nodiscard]] bool backup(
        const std::string& pluginName,
        const std::string& localBackupPath) override;

private:
    Core::Connection::ICommandExecutor& executor_;
};

} // namespace OpenC3::Services
