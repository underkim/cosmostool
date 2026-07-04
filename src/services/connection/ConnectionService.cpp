#include "ConnectionService.h"
#include "core/connection/wsl/WslExecutor.h"
#include "core/connection/ssh/SshExecutor.h"
#include "core/logging/Logger.h"

namespace OpenC3::Services {

ConnectionService::ConnectionService(ISettingsService& settings)
    : settings_(settings)
{}

bool ConnectionService::connect(const std::string& profileId)
{
    setState(ConnectionState::Connecting);

    auto maybeProfile = settings_.profileById(profileId);
    if (!maybeProfile) {
        setState(ConnectionState::Error, "Profile not found: " + profileId);
        return false;
    }

    const auto& profile = *maybeProfile;
    const auto  cfg     = profile.toConfig();

    using namespace Core::Connection;
    if (profile.mode == Models::ConnectionMode::WSL)
        executor_ = std::make_unique<WslExecutor>(cfg);
    else
        executor_ = std::make_unique<SshExecutor>(cfg);

    if (!executor_->connect()) {
        executor_.reset();
        setState(ConnectionState::Error,
                 "Failed to connect to " + profile.name);
        return false;
    }

    activeProfile_ = maybeProfile;  // stored by value — safe after connect() returns
    setState(ConnectionState::Connected);
    Logging::Logger::info("[ConnectionService] Connected via profile '{}'",
                          profile.name);
    return true;
}

void ConnectionService::disconnect()
{
    if (executor_) executor_->disconnect();
    activeProfile_.reset();
    // Fire Disconnected (which resets ExecutorProxy to the null executor,
    // see Application::registerServices()) *before* destroying the real
    // executor object below. Domain services (Docker/System/Doctor/...)
    // hold a stable ExecutorProxy& and may be mid-call from a background
    // thread; reversing this order would let such a call dereference the
    // executor after executor_.reset() has already freed it.
    setState(ConnectionState::Disconnected);
    executor_.reset();
}

ConnectionState ConnectionService::state() const noexcept { return state_; }

const Models::ConnectionProfile*
ConnectionService::activeProfile() const noexcept
{
    return activeProfile_ ? &*activeProfile_ : nullptr;
}

std::string ConnectionService::cosmosRootPath() const noexcept
{
    return activeProfile_ ? activeProfile_->cosmosRootPath : "/cosmos";
}

void ConnectionService::onStateChanged(
    std::function<void(const ConnectionEvent&)> cb)
{
    stateCallbacks_.push_back(std::move(cb));
}

Core::Connection::ICommandExecutor* ConnectionService::executor() const noexcept
{
    return executor_.get();
}

void ConnectionService::setState(ConnectionState s, const std::string& err)
{
    state_ = s;
    ConnectionEvent ev;
    ev.state        = s;
    ev.profileId    = activeProfile_ ? activeProfile_->id : "";
    ev.errorMessage = err;
    for (auto& cb : stateCallbacks_) cb(ev);
}

} // namespace OpenC3::Services
