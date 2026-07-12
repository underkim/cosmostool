#pragma once

#include "models/ConnectionProfile.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace OpenC3::Services {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct ConnectionEvent {
    ConnectionState state;
    std::string     profileId;
    std::string     errorMessage;
};

class IConnectionService {
public:
    /// Token returned by onStateChanged(); pass to removeStateChanged() to
    /// unsubscribe. Subscribers whose lifetime is shorter than the service
    /// (ViewModels) must unsubscribe in their destructor, otherwise a later
    /// state event would invoke a callback whose captured `this` is dangling.
    using SubscriptionId = std::size_t;

    virtual ~IConnectionService() = default;

    [[nodiscard]] virtual bool connect(const std::string& profileId) = 0;
    virtual void               disconnect() = 0;
    [[nodiscard]] virtual ConnectionState state() const noexcept = 0;

    [[nodiscard]] virtual const Models::ConnectionProfile* activeProfile() const noexcept = 0;

    // Returns the COSMOS root path from the active profile, or "/cosmos" if not connected.
    [[nodiscard]] virtual std::string cosmosRootPath() const noexcept = 0;

    virtual SubscriptionId onStateChanged(std::function<void(const ConnectionEvent&)> cb) = 0;
    virtual void           removeStateChanged(SubscriptionId id) = 0;
};

} // namespace OpenC3::Services
