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
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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
        const std::string detail = executor_->lastError();
        executor_.reset();
        setState(ConnectionState::Error,
                 "Failed to connect to " + profile.name + (detail.empty() ? "" : ": " + detail));
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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

ConnectionState ConnectionService::state() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return state_;
}

const Models::ConnectionProfile*
ConnectionService::activeProfile() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return activeProfile_ ? &*activeProfile_ : nullptr;
}

std::string ConnectionService::cosmosRootPath() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return activeProfile_ ? activeProfile_->cosmosRootPath : "/cosmos";
}

IConnectionService::SubscriptionId ConnectionService::onStateChanged(
    std::function<void(const ConnectionEvent&)> cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const SubscriptionId id = nextSubscriptionId_++;
    stateCallbacks_.emplace_back(id, std::move(cb));
    return id;
}

void ConnectionService::removeStateChanged(SubscriptionId id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::erase_if(stateCallbacks_,
                  [id](const auto& entry) { return entry.first == id; });
}

Core::Connection::ICommandExecutor* ConnectionService::executor() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return executor_.get();
}

void ConnectionService::setState(ConnectionState s, const std::string& err)
{
    state_ = s;
    ConnectionEvent ev;
    ev.state        = s;
    ev.profileId    = activeProfile_ ? activeProfile_->id : "";
    ev.errorMessage = err;
    // Iterate a copy: the mutex is recursive, so a callback may legally call
    // removeStateChanged() mid-dispatch, which would otherwise invalidate the
    // iterators of the vector being walked here.
    const auto callbacks = stateCallbacks_;
    for (const auto& [id, cb] : callbacks) cb(ev);
}

} // namespace OpenC3::Services
