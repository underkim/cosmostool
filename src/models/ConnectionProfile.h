#pragma once

#include "core/connection/ConnectionConfig.h"

#include <string>

namespace OpenC3::Models {

using ConnectionMode = Core::Connection::ConnectionMode;
using AuthMethod     = Core::Connection::ConnectionConfig::AuthMethod;

/// Persisted profile representing one connection target.
/// Stored in settings.json and presented in the Settings view.
struct ConnectionProfile {
    std::string    id;        // UUID, generated on creation
    std::string    name;      // Display name, e.g. "Dev Server"
    ConnectionMode mode{ConnectionMode::WSL};
    bool           isDefault{false};

    // ── COSMOS installation ───────────────────────────────────────────────────
    std::string cosmosRootPath{"/cosmos"};  // root of the COSMOS deployment on the target

    // ── WSL ───────────────────────────────────────────────────────────────────
    std::string wslDistribution{"Ubuntu"};

    // ── SSH ───────────────────────────────────────────────────────────────────
    std::string host;
    int         port{22};
    std::string username;
    AuthMethod  authMethod{AuthMethod::PublicKey};
    std::string password;           // stored encrypted in settings
    std::string privateKeyPath;
    std::string publicKeyPath;
    std::string passphrase;

    int connectTimeoutMs{10'000};
    int commandTimeoutMs{30'000};

    [[nodiscard]] Core::Connection::ConnectionConfig toConfig() const
    {
        Core::Connection::ConnectionConfig cfg;
        cfg.mode             = mode;
        cfg.cosmosRootPath   = cosmosRootPath;
        cfg.wslDistribution  = wslDistribution;
        cfg.host             = host;
        cfg.port             = port;
        cfg.username         = username;
        cfg.authMethod       = authMethod;
        cfg.password         = password;
        cfg.privateKeyPath   = privateKeyPath;
        cfg.publicKeyPath    = publicKeyPath;
        cfg.passphrase       = passphrase;
        cfg.connectTimeoutMs = connectTimeoutMs;
        cfg.commandTimeoutMs = commandTimeoutMs;
        return cfg;
    }
};

} // namespace OpenC3::Models
