#pragma once

#include "IWorkspaceService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

/// IWorkspaceService over the shell transport (WSL/SSH).
///
/// This is the former PluginService, renamed and narrowed. The install() and
/// remove() methods it used to carry were removed rather than moved: they
/// copied/deleted a .gem under `<cosmosRoot>/plugins/` and reported success,
/// but that has no effect whatsoever on a running COSMOS - installation is a
/// two-phase server-side operation (see CosmosPluginService). Keeping them
/// would have preserved a button that silently did nothing.
class WorkspaceService final : public IWorkspaceService {
public:
    explicit WorkspaceService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] std::vector<Models::Plugin>
        listSources(const std::string& cosmosRoot) override;

    [[nodiscard]] Models::PluginValidationResult
        validate(const std::string& localPluginPath) override;

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
