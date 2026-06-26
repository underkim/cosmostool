#pragma once

#include "models/ConnectionProfile.h"

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
    virtual ~IConnectionService() = default;

    [[nodiscard]] virtual bool connect(const std::string& profileId) = 0;
    virtual void               disconnect() = 0;
    [[nodiscard]] virtual ConnectionState state() const noexcept = 0;

    [[nodiscard]] virtual const Models::ConnectionProfile* activeProfile() const noexcept = 0;

    virtual void onStateChanged(std::function<void(const ConnectionEvent&)> cb) = 0;
};

} // namespace OpenC3::Services
