#pragma once

#include "IConnectionService.h"
#include "core/connection/ICommandExecutor.h"
#include "services/settings/ISettingsService.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace OpenC3::Services {

class ConnectionService final : public IConnectionService {
public:
    explicit ConnectionService(ISettingsService& settings);

    [[nodiscard]] bool connect(const std::string& profileId) override;
    void               disconnect() override;
    [[nodiscard]] ConnectionState state() const noexcept override;

    [[nodiscard]] const Models::ConnectionProfile* activeProfile() const noexcept override;
    [[nodiscard]] std::string cosmosRootPath() const noexcept override;

    void onStateChanged(std::function<void(const ConnectionEvent&)> cb) override;

    /// Provides access to the active executor for services that need to
    /// run commands (DockerService, SystemService, etc.).
    [[nodiscard]] Core::Connection::ICommandExecutor* executor() const noexcept;

private:
    void setState(ConnectionState s, const std::string& err = {});

    ISettingsService&                                 settings_;
    std::unique_ptr<Core::Connection::ICommandExecutor> executor_;
    ConnectionState                                   state_{ConnectionState::Disconnected};
    std::optional<Models::ConnectionProfile>          activeProfile_;

    std::vector<std::function<void(const ConnectionEvent&)>> stateCallbacks_;
};

} // namespace OpenC3::Services
