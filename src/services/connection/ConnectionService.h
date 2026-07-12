#pragma once

#include "IConnectionService.h"
#include "core/connection/ICommandExecutor.h"
#include "services/settings/ISettingsService.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
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

    SubscriptionId onStateChanged(std::function<void(const ConnectionEvent&)> cb) override;
    void           removeStateChanged(SubscriptionId id) override;

    /// Provides access to the active executor for services that need to
    /// run commands (DockerService, SystemService, etc.).
    [[nodiscard]] Core::Connection::ICommandExecutor* executor() const noexcept;

private:
    void setState(ConnectionState s, const std::string& err = {});

    ISettingsService&                                 settings_;
    std::unique_ptr<Core::Connection::ICommandExecutor> executor_;
    ConnectionState                                   state_{ConnectionState::Disconnected};
    std::optional<Models::ConnectionProfile>          activeProfile_;

    std::vector<std::pair<SubscriptionId,
                          std::function<void(const ConnectionEvent&)>>> stateCallbacks_;
    SubscriptionId nextSubscriptionId_{1};

    // connect() runs on a background thread (SettingsViewModel::connectToProfile
    // via QtConcurrent::run) while disconnect() is invoked synchronously from
    // the GUI thread - without this, rapid Connect-then-Disconnect clicks (or
    // even just clicking Disconnect before the "Connecting" UI-disable round
    // trip lands) let both run concurrently and mutate executor_/state_/
    // activeProfile_ unsynchronized. Recursive because setState() invokes
    // stateCallbacks_ synchronously while already locked, and those callbacks
    // (see Application::registerServices()) call back into executor().
    mutable std::recursive_mutex mutex_;
};

} // namespace OpenC3::Services
